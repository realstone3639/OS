#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 4096

/* 내부 명령어 판별 */
int is_builtin(const char *s) { return strcmp(s, "cd") == 0 || strcmp(s, "exit") == 0; }

/* 공백 기준 파싱 – execvp용 인수 배열 */
void parse(char *src, char **argv) {
    int i = 0;
    argv[i] = strtok(src, " ");
    while (argv[i] && i < 63) argv[++i] = strtok(NULL, " ");
    argv[i] = NULL;
}

int main(void) {
    char line[512];

    while (1) {
        printf("[myshell] ");
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;

        /* command1 , command2 , outfile 분리 */
        char *tok[3] = { strtok(line, ","), NULL, NULL };
        tok[1] = strtok(NULL, ",");
        tok[2] = strtok(NULL, ",");
        if (!tok[0] || !tok[1] || !tok[2]) {
            fprintf(stderr, "Usage: cmd1, cmd2, outfile\n");
            continue;
        }
        for (int i = 1; i < 3; ++i)
            while (tok[i] && *tok[i] == ' ') ++tok[i];

        /* outfile 열기 (append) */
        int fd_out = open(tok[2], O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_out < 0) { perror("open"); continue; }

        /* builtin 처리 */
        if (is_builtin(tok[0])) {
            printf("  %s: builtin command\n", tok[0]);
            close(fd_out);
            continue;
        }

        /* 인수 배열 준비 */
        char *argv1[64], *argv2[64];
        parse(tok[0], argv1);
        parse(tok[1], argv2);

        /* 파이프 두 개 */
        int p1[2], p2[2];
        if (pipe(p1) || pipe(p2)) { perror("pipe"); close(fd_out); continue; }

        /* ---------- child 1 : command_1 ---------- */
        pid_t c1 = fork();
        if (c1 == 0) {
            dup2(p1[1], STDOUT_FILENO);           /* cmd1 stdout -> p1 */
            close(p1[0]); close(p1[1]);
            close(p2[0]); close(p2[1]);
            execvp(argv1[0], argv1);
            perror("execvp cmd1"); _exit(1);
        }

        /* 헤더: After executing cmd1 */
        dprintf(fd_out, "After executing '%s'\n", tok[0]);

        /* ---------- child 2 : command_2 ---------- */
        pid_t c2 = fork();
        if (c2 == 0) {
            dup2(p2[0], STDIN_FILENO);            /* cmd2 stdin  <- p2 */
            /* stdout: 터미널 + 파일 모두 */
            int saved = dup(STDOUT_FILENO);       /* 터미널 복사 */
            dup2(fd_out, STDOUT_FILENO);          /* stdout -> outfile */
            /* stderr 그대로 (화면) */
            close(p1[0]); close(p1[1]);
            close(p2[0]); close(p2[1]);
            execvp(argv2[0], argv2);              /* builtin이면 exec 없이 종료 가능 */
            perror("execvp cmd2"); _exit(1);
        }

        /* 헤더 미리 기록 (cmd2 시작 전) */
        dprintf(fd_out, "After executing '%s'\n", tok[1]);

        /* parent : 중계자 역할 */
        close(p1[1]);          /* 읽기만 */
        close(p2[0]);          /* 쓰기만 */
        char buf[BUF_SIZE];
        ssize_t n;
        while ((n = read(p1[0], buf, sizeof buf)) > 0) {
            write(fd_out, buf, n);   /* outfile로 */
            write(p2[1], buf, n);    /* cmd2로   */
        }
        close(p1[0]);
        close(p2[1]);          /* EOF 알림 */

        /* 자식 종료 대기 */
        int st;
        waitpid(c1, &st, 0);
        waitpid(c2, &st, 0);

        dprintf(fd_out, "Done\n");
        close(fd_out);
    }
    return 0;
}
