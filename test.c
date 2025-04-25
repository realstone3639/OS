#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // fork(), exec(), pipe(), dup2()
#include <string.h>
#include <fcntl.h>      // open()
#include <sys/wait.h>   // wait()
#include <errno.h>

#define MAX 1024

// 내장 명령어인지 판단 (cd, exit만 해당)
int isBuiltIn(char* cmd) {
    return (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0);
}

// 문자열 input을 공백 기준으로 쪼개어 args 배열에 저장
void parse(char* input, char* args[]) {
    int i = 0;
    args[i] = strtok(input, " ");
    while (args[i] != NULL && i < MAX) {
        i++;
        args[i] = strtok(NULL, " ");
    }
}

// 쉘 입력 문자열을 ',' 기준으로 3개로 분리하여 parts 배열에 저장
// parts[0] = command1, parts[1] = command2, parts[2] = outfile
int split_input(char* line, char* parts[3]) {
    int i = 0;
    parts[i] = strtok(line, ",");
    while (parts[i] != NULL && i < 3) {
        // 각 명령어 앞뒤 공백 제거 및 개행 제거
        parts[i] = strtok(parts[i], "\n");
        while (*parts[i] == ' ') parts[i]++;
        i++;
        parts[i] = strtok(NULL, ",");
    }
    return i;  // 분할된 문자열 개수 반환
}

int main() {
    char line[MAX];      // 사용자 입력
    char* part[3];       // command1, command2, outfile

    while (1) {
        printf("[myshell] ");
        if (fgets(line, sizeof(line), stdin) == NULL) break;

        // "exit" 명령 처리
        if (strncmp(line, "exit", 4) == 0) break;

        // 입력 문자열을 3개 부분으로 나눔
        int count = split_input(line, part);
        if (count != 3) {
            fprintf(stderr, "Usage: command1, command2, outfile\n");
            continue;
        }

        // 명령어와 인자 분리
        char* cmd1_args[MAX], *cmd2_args[MAX];
        char cmd1_str[MAX], cmd2_str[MAX];
        strcpy(cmd1_str, part[0]);
        strcpy(cmd2_str, part[1]);

        parse(cmd1_str, cmd1_args);  // command1 파싱
        parse(cmd2_str, cmd2_args);  // command2 파싱

        // 각 명령이 내장 명령인지 판단
        int builtin1 = isBuiltIn(cmd1_args[0]);
        int builtin2 = isBuiltIn(cmd2_args[0]);

        // command1이 내장 명령이면 실행하지 않고 알림 출력
        if (builtin1) {
            printf("%s: builtin command\n", cmd1_args[0]);
            continue;
        }

        // outfile을 열기 (append 모드)
        int outfile_fd = open(part[2], O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (outfile_fd < 0) {
            perror("outfile open");
            continue;
        }

        // pipe 생성 (무기명 파이프)
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            close(outfile_fd);
            continue;
        }

        // 자식 프로세스 1: command1 실행
        pid_t pid1 = fork();
        if (pid1 == 0) {
            close(pipefd[0]);                     // 읽기 닫기
            dup2(pipefd[1], STDOUT_FILENO);       // stdout -> 파이프 쓰기쪽
            close(pipefd[1]);

            execvp(cmd1_args[0], cmd1_args);      // command1 실행
            perror("command_1");                  // 실패 시 에러 출력
            exit(1);
        }

        // 자식 프로세스 2: command2 실행 및 출력 관리
        pid_t pid2 = fork();
        if (pid2 == 0) {
            close(pipefd[1]);                     // 쓰기 닫기
            dup2(pipefd[0], STDIN_FILENO);        // stdin <- 파이프 읽기쪽
            close(pipefd[0]);

            // command2의 결과를 화면과 outfile에 동시에 출력
            FILE* pipe_in = fdopen(STDIN_FILENO, "r");            // 파이프에서 읽기
            FILE* out_fp = fdopen(dup(outfile_fd), "a");          // outfile에 쓰기 (복사한 파일 디스크립터)

            // command2가 내장 명령어이면 실행하지 않고 메시지만 출력
            if (builtin2) {
                printf("%s: builtin command\n", cmd2_args[0]);
            } else {
                // command2를 실행할 또 다른 프로세스 생성 (stdout을 pipefd2로 리다이렉트)
                int pipefd2[2];
                pipe(pipefd2);
                pid_t pid3 = fork();
                if (pid3 == 0) {
                    dup2(pipefd[0], STDIN_FILENO);
                    dup2(pipefd2[1], STDOUT_FILENO);     // stdout -> pipefd2
                    close(pipefd2[0]);
                    close(pipefd2[1]);
                    execvp(cmd2_args[0], cmd2_args);
                    perror("command_2");
                    exit(1);
                } else {
                    // pipefd2로부터 읽어서 화면 + 파일 동시에 출력
                    close(pipefd2[1]);
                    FILE* from_child = fdopen(pipefd2[0], "r");
                    char line[256];
                    while (fgets(line, sizeof(line), from_child)) {
                        fputs(line, stdout);     // 화면 출력
                        fputs(line, out_fp);     // 파일 출력
                    }
                    fclose(from_child);
                }
            }

            // command2 완료 표시
            fprintf(out_fp, "Done\n");
            fclose(pipe_in);
            fclose(out_fp);
            exit(0);
        }

        // 부모 프로세스: 파일에 헤더 작성 및 자식 종료 대기
        dprintf(outfile_fd, "After executing '%s'\n", part[0]);
        dprintf(outfile_fd, "After executing '%s'\n", part[1]);

        // 파이프 및 파일 디스크립터 정리
        close(pipefd[0]);
        close(pipefd[1]);
        close(outfile_fd);

        // 자식 프로세스 종료 대기
        wait(NULL);
        wait(NULL);
    }

    return 0;
}
