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
        char *tmpfile = "tmpfile.txt";

        parse_command(cmd[0], argv1);
        parse_command(cmd[1], argv2);

        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
            perror("open outfile");
            continue;
        }

        int fd_tmp = open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_tmp < 0) {
            perror("open tmpfile");
            close(fd_out);
            continue;
        }

        // After executing 'ls -al /tmp'
        dprintf(fd_out, "After executing '%s'\n", cmd[0]);

        // 1. ls 실행해서 tmpfile에 저장
        pid_t pid1 = fork();
        if (pid1 == 0) {
            dup2(fd_tmp, STDOUT_FILENO); // tmpfile로 저장
            close(fd_tmp);
            execvp(argv1[0], argv1);
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }
        close(fd_tmp);
        waitpid(pid1, NULL, 0);

        // 2. tmpfile 읽어서 outfile에 기록
        int fd_tmp_read = open(tmpfile, O_RDONLY);
        if (fd_tmp_read < 0) {
            perror("open tmpfile read");
            close(fd_out);
            continue;
        }

        char buf[4096];
        ssize_t n;
        while ((n = read(fd_tmp_read, buf, sizeof(buf))) > 0) {
            write(fd_out, buf, n);
        }

        // After executing 'sort -n'
        dprintf(fd_out, "After executing '%s'\n", cmd[1]);

        // 3. sort 실행
        lseek(fd_tmp_read, 0, SEEK_SET); // tmpfile 처음으로 되돌리기
        pid_t pid2 = fork();
        if (pid2 == 0) {
            dup2(fd_tmp_read, STDIN_FILENO);
            dup2(fd_out, STDOUT_FILENO);
            close(fd_tmp_read);
            close(fd_out);
            execvp(argv2[0], argv2);
            perror("execvp cmd2");
            exit(EXIT_FAILURE);
        }
        close(fd_tmp_read);
        waitpid(pid2, NULL, 0);

        // Done
        dprintf(fd_out, "Done\n");
        close(fd_out);
        unlink(tmpfile);
    }
}
