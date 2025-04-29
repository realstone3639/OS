#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

int isBuiltin(char *cmd) {
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "pwd") == 0);
}

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

        char in[100];
        if (fgets(in, sizeof(in), stdin) == NULL) {
            break;
        }

        in[strcspn(in, "\n")] = 0;

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

        char *fileName = cmd[2];
        char *argv1[64];
        char *argv2[64];

        parse_command(cmd[0], argv1);
        parse_command(cmd[1], argv2);

        if (strcmp(argv1[0], "exit") == 0) {
            return 0;
        }

        if (isBuiltin(argv1[0])) {
            printf("%s: builtin command\n", argv1[0]);
            continue;
        }

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            continue;
        }

        int fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            perror("open outfile");
            continue;
        }

        pid_t pid1 = fork();
        if (pid1 == 0) {
            // 첫 번째 자식
            close(pipefd[0]);

            // "After executing 'command_1'" 출력
            dprintf(fd, "After executing '%s'\n", cmd[0]);

            // 파일로 출력 복제
            dup2(pipefd[1], STDOUT_FILENO);

            close(pipefd[1]);
            execvp(argv1[0], argv1);
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }

        pid_t pid2 = fork();
        if (pid2 == 0) {
            // 두 번째 자식
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);

            // 파일에 'After executing command_2' 출력
            dprintf(fd, "After executing '%s'\n", cmd[1]);

            dup2(fd, STDOUT_FILENO);

            execvp(argv2[0], argv2);
            perror("execvp cmd2");
            exit(EXIT_FAILURE);
        }

        // 부모
        close(pipefd[0]);
        close(pipefd[1]);

        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);

        // Done 출력
        dprintf(fd, "Done\n");
        close(fd);
    }
}
