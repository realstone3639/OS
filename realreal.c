#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

void parse_command(char* input, char** argv) {
    int i = 0;
    argv[i] = strtok(input, " ");
    while (argv[i] != NULL) {
        i++;
        argv[i] = strtok(NULL, " ");
    }
}

int main() {
    while (1) {
        printf("[myshell]");

        char in[256];
        if (fgets(in, sizeof(in), stdin) == NULL) {
            break;
        }
        in[strcspn(in, "\n")] = 0;

        if (strcmp(in, "exit") == 0) {
            return 0;
        }

        char* cmd[3];
        cmd[0] = strtok(in, ",");
        for (int i = 1; i < 3; i++) {
            cmd[i] = strtok(NULL, ",");
            if (cmd[i] != NULL) {
                while (*cmd[i] == ' ') cmd[i]++;
            }
        }

        if (!cmd[0] || !cmd[1] || !cmd[2]) {
            fprintf(stderr, "Usage: command1, command2, outfile (comma separated)\n");
            continue;
        }

        char *outfile = cmd[2];
        char *argv1[64];
        char *argv2[64];
        parse_command(cmd[0], argv1);
        parse_command(cmd[1], argv2);

        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
            perror("open outfile");
            continue;
        }

        // "After executing ls" 먼저 기록
        dprintf(fd_out, "After executing '%s'\n", cmd[0]);

        int pipe_ls[2];
        if (pipe(pipe_ls) == -1) {
            perror("pipe_ls");
            close(fd_out);
            continue;
        }

        pid_t pid_ls = fork();
        if (pid_ls == 0) {
            // ls 자식
            close(pipe_ls[0]); // read end 닫고
            dup2(pipe_ls[1], STDOUT_FILENO); // stdout을 pipe로
            close(pipe_ls[1]);
            execvp(argv1[0], argv1);
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }

        close(pipe_ls[1]); // 부모는 write end 닫음

        // pipe에서 읽어오기
        char buf[4096];
        ssize_t n;

        int pipe_sort[2];
        if (pipe(pipe_sort) == -1) {
            perror("pipe_sort");
            close(fd_out);
            close(pipe_ls[0]);
            continue;
        }

        pid_t pid_sort = fork();
        if (pid_sort == 0) {
            // sort 자식
            close(pipe_sort[1]); // read end만 필요
            dup2(pipe_sort[0], STDIN_FILENO); // stdin을 pipe_sort로
            dup2(fd_out, STDOUT_FILENO); // stdout을 outfile로
            close(pipe_sort[0]);
            close(fd_out);
            execvp(argv2[0], argv2);
            perror("execvp cmd2");
            exit(EXIT_FAILURE);
        }

        close(pipe_sort[0]); // 부모는 sort read 닫음

        // 부모는 pipe_ls에서 읽고
        // - fd_out에 쓰고
        // - pipe_sort write로도 쓰기
        while ((n = read(pipe_ls[0], buf, sizeof(buf))) > 0) {
            write(fd_out, buf, n);        // outfile에 ls 결과 저장
            write(pipe_sort[1], buf, n);  // sort stdin으로도 전달
        }

        close(pipe_ls[0]);
        close(pipe_sort[1]); // sort 입력 완료했으면 닫아줘야 함

        waitpid(pid_ls, NULL, 0);

        // After executing 'sort -n'
        dprintf(fd_out, "After executing '%s'\n", cmd[1]);

        waitpid(pid_sort, NULL, 0);

        dprintf(fd_out, "Done\n");

        close(fd_out);
    }
}
