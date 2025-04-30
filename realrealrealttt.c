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
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "echo") == 0 || strcmp(cmd, "pwd") == 0);
}
void execute_builtin(char** argv) {
    if (strcmp(argv[0], "cd") == 0) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing operand\n");
        } else {
            if (chdir(argv[1]) != 0) {
                perror("cd");
            }
        }
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd");
        }
    } else if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; argv[i] != NULL; i++) {
            printf("%s ", argv[i]);
        }
        printf("\n");
    }
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

        char in[256];
        if (fgets(in, sizeof(in), stdin) == NULL) {
            continue;
        }
        in[strcspn(in, "\n")] = 0;

        if (strcmp(in, "exit") == 0) {
            return 0;
        }

        char* cmd[3] = { NULL };
        cmd[0] = strtok(in, ",");
        for (int i = 1; i < 3; i++) {
            cmd[i] = strtok(NULL, ",");
            if (cmd[i] != NULL) {
                while (*cmd[i] == ' ') cmd[i]++;
            }
        }
        int cmd_count = 0;
        for (int i = 0; i < 3; i++) {
            if (cmd[i] != NULL) cmd_count++;
        }

        if (cmd_count != 3) {
            fprintf(stderr, "Usage: command1, command2, outfile (comma separated)\n");
            continue;
        }

        char *outfile = cmd[2];
        char *argv1[64];
        char *argv2[64];

        parse_command(cmd[0], argv1);
        parse_command(cmd[1], argv2);

        //마지막 파일 여는부분
        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_out < 0) {
            perror("open outfile");
            continue;
        }

        // After executing 'ls -al /tmp'
        dprintf(fd_out, "After executing '%s'\n", cmd[0]);

        // 명령어 1 빌트인 명령어 검사
        if (isBuiltin(argv1[0]))
        {
            printf("%s: bulitin command", argv1[0]);
            continue;
        }
        
        // 1. ls 실행해서 outfile에 저장
        pid_t pid1 = fork();
        if (pid1 == 0) {
            dup2(fd_out, STDOUT_FILENO); 
            close(fd_out);
            execvp(argv1[0], argv1);
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }
        waitpid(pid1, NULL, 0);
        
        //2. pid 3으로 파이프로 보내기기
        int pipe2[2];
        if (pipe(pipe2) < 0) {
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
        
        pid_t pid2 = fork();
        if (pid2 < 0) {
            perror("fork pid2");
            exit(EXIT_FAILURE);
        }
        
        if (pid2 == 0) {
            close(pipe2[0]);
            dup2(pipe2[1], STDOUT_FILENO);
            close(pipe2[1]);
            execvp(argv1[0], argv1);
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }

        // After executing 'sort -n'
        dprintf(fd_out, "After executing '%s'\n", cmd[1]);

        // 3. sort 실행
        pid_t pid3 = fork();
        if (pid3 < 0) {
            perror("fork pid3");
            exit(EXIT_FAILURE);
        }
        
        if (pid3 == 0) {
            if (isBuiltin(argv2[0]))
            {
                execute_builtin(argv2);
                exit(EXIT_SUCCESS);
            }
            else{
                close(pipe2[1]);
                dup2(pipe2[0], STDIN_FILENO);
                dup2(fd_out, STDOUT_FILENO);
                close(pipe2[0]);
                close(fd_out);
                execvp(argv2[0], argv2);
                perror("execvp cmd2");
                exit(EXIT_FAILURE);
            }
        }
        close(pipe2[0]);
        close(pipe2[1]);
        waitpid(pid2, NULL, 0);
        waitpid(pid3, NULL, 0);
        // Done
        dprintf(fd_out, "Done\n");
        close(fd_out);
    }
}
