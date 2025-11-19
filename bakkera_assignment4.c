/**
 * A sample program for parsing a command line. If you find it useful,
 * feel free to adapt this code for Assignment 4.
 * Do fix memory leaks and any additional issues you find.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> // pid_t
#include <sys/wait.h>
#include <fcntl.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS	512

int last_status = 0;

struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	char *trimmed = input;
	while (*trimmed == ' ' || *trimmed == '\t') {
		trimmed++;
	}
		if (*trimmed == '\n' || *trimmed == '\0' || *trimmed == '#') {
			free(curr_command);
			return NULL;
	}

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token){
		if(!strcmp(token,"<")){
			curr_command->input_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,">")){
			curr_command->output_file = strdup(strtok(NULL," \n"));
		} else if(!strcmp(token,"&")){
			curr_command->is_bg = true;
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

void handle_cd(struct command_line *cmd) {
	char *target = cmd->argv[1];

	// If there is no argument, goes to HOME direectory
	if (!target) {
		target = getenv("HOME");
	}

	// Changes the shell's working directory
	if (chdir(target) != 0) {
		perror("cd");
	}
}

void handle_status() {
	printf("exit value %d\n", last_status);
}

int main()
{
	struct command_line *curr_command;

	while(true)
	{
		curr_command = parse_input();

		if (curr_command == NULL) {
			continue;
		}

		if (strcmp(curr_command->argv[0], "exit") == 0) {
			free(curr_command);
			exit(0);
		} else if (strcmp(curr_command->argv[0], "cd") == 0) {
			handle_cd(curr_command);
		} else if (strcmp(curr_command->argv[0], "status") == 0) {
			handle_status();
		} else {
			// Non built-in commands 
			pid_t child_pid = fork();
			if (child_pid == -1) {
				perror("fork");
				last_status = 1;
			} else if (child_pid == 0) {	
				if (curr_command->is_bg) {
					if (!curr_command->input_file) {
						int fd_in = open("/dev/null", O_RDONLY);
						dup2(fd_in, 0);
						close(fd_in);
					} 
					
					if (!curr_command->output_file) {
						int fd_out = open("/dev/null", O_WRONLY);
						dup2(fd_out, 1);
						close(fd_out);
					}
				}
				// Input redirection
				if (curr_command->input_file) {
					int fd_in = open(curr_command->input_file, O_RDONLY);
					if (fd_in == -1) {
						printf("cannot open %s for input\n", curr_command->input_file);
						exit(1);
					} else if (dup2(fd_in, 0) == -1) {
						perror("dup2 input");
						exit(2);
					}
						close(fd_in);
				}
				
				// Output redirection
				if (curr_command->output_file) {
					int fd_out = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (fd_out == -1) {
						printf("cannot open %s for output\n", curr_command->output_file);
						exit(1);
					} else if (dup2(fd_out, 1) == -1) {
						perror("dup2 output");
						exit(2);
					}
					close(fd_out);
				}
					execvp(curr_command->argv[0], curr_command->argv);
					perror("execvp");
					exit(1);
			} else {
				// Parent process
				if (!curr_command->is_bg) {

					int child_status;
					waitpid(child_pid, &child_status, 0);
					
					if (WIFEXITED(child_status)) {
						last_status = WEXITSTATUS(child_status);
					} else if (WIFSIGNALED(child_status)) {
						printf("terminated by signal %d\n", WTERMSIG(child_status));
						fflush(stdout);
						last_status = WTERMSIG(child_status);
					}
				} else {
					printf("background pid is %d\n", child_pid);
				}
			}
		}

		int child_status;
		pid_t finished_pid;

		while ((finished_pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
			if (WIFEXITED(child_status)) {
				printf("background pid %d is done: exit value %d\n", finished_pid, WEXITSTATUS(child_status));
			} else if (WIFSIGNALED(child_status)) {
				printf("background pid %d is done: terminated by signal %d\n", finished_pid, WTERMSIG(child_status));
			}
			fflush(stdout);
		}
	

		for (int i = 0; i < curr_command->argc; i++) {
			free(curr_command->argv[i]);
		}
		free(curr_command->input_file);
		free(curr_command->output_file);
		free(curr_command);

	}
	return EXIT_SUCCESS;
}