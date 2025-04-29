#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
//#include <unistd.h>
#include <string.h>
//#include <sys/wait.h>
#include <stdlib.h>

int isBuiltIn(char** cmd) {
	if (cmd[0] == "cd" || cmd[0] == "exit")
	{
		return 1;
	}
	else {
		return 0;
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
	while (1)
	{
		printf("[myshell]");
		char in[100];
		fgets(in, sizeof(in), stdin);
		
		char* cmd[3];
		char* cmd1[3];
		char* cmd2[3];

		cmd[0] = strtok(in, ",");
		for (int i = 1; i < 3; i++) {
			cmd[i] = strtok(NULL, ",");
			if (cmd[i] != NULL) {
				while (*cmd[i] == ' ') cmd[i]++;
			}
		}
		
		if (isBuiltIn(cmd1)
		{
			printf("%s: builtin command", cmd1[0]);
			
		}
	}
}