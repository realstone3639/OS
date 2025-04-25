
// file: mypipe.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_CMD 256
#define MAX_ARGS 64

int is_builtin(const char *cmd) {
    return strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0;
}

void parse_command(char *input, char **cmd1, char **cmd2, char **outfile) {
    char *token = strtok(input, ",");
    *cmd1 = token ? strdup(token) : NULL;
    token = strtok(NULL, ",");
    *cmd2 = token ? strdup(token) : NULL;
    token = strtok(NULL, ",");
    *outfile = token ? strdup(token) : NULL;
}

char **tokenize_args(char *cmd) {
    static char *args[MAX_ARGS];
    int i = 0;
    char *token = strtok(cmd, " \t\n");
    while (token && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return args;
}

int main() {
    char line[MAX_CMD];

    while (1) {
        printf("[myshell] ");
        fflush(stdout);
        if (!fgets(line, MAX_CMD, stdin)) break;

        // trim newline
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "exit") == 0) break;

        char *cmd1 = NULL, *cmd2 = NULL, *outfile = NULL;
        parse_command(line, &cmd1, &cmd2, &outfile);

        if (!cmd1 || !cmd2 || !outfile) {
            fprintf(stderr, "Usage: command_1 –op args, command_2 –op args, outfile\n");
            continue;
        }

        if (is_builtin(cmd1)) {
            printf("%s: builtin command\n", strtok(cmd1, " \t"));
            continue;
        }

        int pipefd[2];
        pipe(pipefd);

        pid_t pid1 = fork();
        if (pid1 == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
            char **args1 = tokenize_args(cmd1);
            execvp(args1[0], args1);
            perror("execvp cmd1");
            exit(1);
        }

        pid_t pid2 = fork();
        if (pid2 == 0) {
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);

            int fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) {
                perror("open outfile");
                exit(1);
            }

            int backup = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);  // write to file
            char **args2 = tokenize_args(cmd2);

            FILE *fp = popen(cmd2, "r");
            if (fp) {
                char buf[1024];
                dprintf(fd, "After executing ‘%s’\n", cmd2);
                dprintf(backup, "After executing ‘%s’\n", cmd2);
                while (fgets(buf, sizeof(buf), fp)) {
                    write(fd, buf, strlen(buf));
                    write(backup, buf, strlen(buf));
                }
                dprintf(fd, "Done\n");
                dprintf(backup, "Done\n");
                pclose(fp);
            } else {
                perror("popen cmd2");
            }

            close(fd);
            exit(0);
        }

        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);

        free(cmd1); free(cmd2); free(outfile);
    }

    return 0;
}
