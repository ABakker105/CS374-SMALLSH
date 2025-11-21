/*
    Program Name: bakkera_assignment4.c
    Author: Allessandra Bakker
    Email: bakkera@oregonstate.edu

    Description: For this assignment, I wrote my own shell in C called smallsh. My program provides a prompt for 
	running commands, handles blank lines and comments, executes 3 commands (exit, cd, and status) via code built into the shell, 
	executes other commands by creating new processes using a function from the exec() family of functions, 
	supports input and output redirection, supports running commands in foreground and background processes, and implements custom handlers for 2 signals, SIGINT and SIGTSTP.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS	512

int last_status = 0;
int fg_only_mode = 0;

/*
	Struct: command_line

	argv - Argument list (NULL-terminated)
	argc - Number of arguments
	input_file - Filename after "<" or NULL
	output_file - Filename after ">" or NULL
	is_bg - True if command ends with "&"
*/
struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};

/*
	Function: free_command

	Description: Frees memory to prevent memory leaks
*/
void free_command(struct command_line *cmd) {
	if (cmd == NULL) {
		return;
	}

	for (int i = 0; i < cmd->argc; i++) {
		free(cmd->argv[i]);
	}
		free(cmd->input_file);
		free(cmd->output_file);
		free(cmd);
}

/*
	Function: parse_input

	Description: Reads a line of input from the user, parses it into tokens, and constructs a command_line struct 
	describing the command. 

	Returns: 
		- Pointer to a dynamically allocated commmand_line struct representing the parsed command
		- NULL if the line is empty or a comment
*/
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
			free_command(curr_command);
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
			if (fg_only_mode == 0) {
				curr_command->is_bg = true;
			}
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

/* 
	Function: handle_SIGTSTP

	Parameter(s): 
		- signo: the signal number

	Description: Signal handler for SIGTSTP. This function toggles the shell's 
	"foreground-only mode." when foreground-only mode is enabled, any command ending with 
	"&" is ignored and run in the foreground instead. 
*/
void handle_SIGTSTP(int signo) {
	(void)signo;
	const char msg_on[] = "Entering foreground-only mode (& is now ignored)\n";
	const char msg_off[] = "Exiting foreground-only mode\n";

	if (fg_only_mode == 0) {
		fg_only_mode = 1;
		write(STDOUT_FILENO, msg_on, sizeof(msg_on) - 1);
	} else {
		fg_only_mode = 0;
		write(STDOUT_FILENO, msg_off, sizeof(msg_off) - 1);
	}
}

/*
	Function: setup_parent_signals

	Description: 
		- SIGINT is ignored so the shell itself is not killed
		- SIGTSTP uses handle _SIGTSTP() to toggle foreground-only mode.

*/
void setup_parent_signals() {
	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGINT_action = {0};
	// Ignores SIGINT in the shell
	SIGINT_action.sa_handler = SIG_IGN;
	// Blocks all signals during handler
	sigfillset(&SIGINT_action.sa_mask);
	sigaction(SIGINT, &SIGINT_action, NULL);

	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Blocks all signals during handler
	sigfillset(&SIGTSTP_action.sa_mask);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

/*
	Function: reap_background

	Description: Checks for any completed background child processes without blocking.
*/
void reap_background() {
	int child_status;
	pid_t reap_pid;

	// Loops through all child processes that have finished.
	while ((reap_pid = waitpid(-1, &child_status, WNOHANG)) > 0) {
		if (WIFEXITED(child_status)) {
			// Background child process ended normally with an exit value.
			printf("background pid %d is done: exit value %d\n", reap_pid, WEXITSTATUS(child_status));
		} else if (WIFSIGNALED(child_status)) {
			// Background child process was terminated by a signal.
			printf("background pid %d is done: terminated by signal %d\n", reap_pid, WTERMSIG(child_status));
		}
			fflush(stdout);
	}
}

/*
	Function: handle_cd

	Description: Implements the built-in "cd" command for the shell.
*/
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

/*
	Function: handle_status

	Description: Implements the built_in "status" command for the shell.
*/
void handle_status() {
	// Checks if the foreground process exited normally.
	if (WIFEXITED(last_status)) {
		printf("exit value %d\n", WEXITSTATUS(last_status));
	// Checks if the process was terminated by a signal.
	} else if (WIFSIGNALED(last_status)) {
		printf("terminated by signal %d\n", WTERMSIG(last_status));
	}
	fflush(stdout);
}

/* 
	Function: main

	Description: Runs the shell by setting up signals, reading user input, handling built-in commands, 
	and forking child processes to execute non-built-in commands with support for redirection and background jobs.
*/
int main()
{
	setup_parent_signals();
	struct command_line *curr_command;

	while(true)
	{
		reap_background();
		curr_command = parse_input();

		if (curr_command == NULL) {
			continue;
		}
		// Checks if command entered is "exit" and exits the program if that's the case
		if (strcmp(curr_command->argv[0], "exit") == 0) {
			free_command(curr_command);
			exit(0);
		// Checks if the command entered is "cd" and changes the directory if that's the case
		} else if (strcmp(curr_command->argv[0], "cd") == 0) {
			handle_cd(curr_command);
		// Checks if the command entered is "status" and prints the status if that's the case
		} else if (strcmp(curr_command->argv[0], "status") == 0) {
			handle_status();
		} else {
			// Forks a child process for non built-in commands 
			pid_t child_pid = fork();
			
			if (child_pid == -1) {
				perror("fork");
				last_status = 1;
			} else if (child_pid == 0) {
				// Child process	
				struct sigaction SIGTSTP_action = {0};
				SIGTSTP_action.sa_handler = SIG_IGN;
				sigaction(SIGTSTP, &SIGTSTP_action, NULL);

				struct sigaction SIGINT_action = {0};
				if (curr_command->is_bg) {
					SIGINT_action.sa_handler = SIG_IGN;
				} else {
					SIGINT_action.sa_handler = SIG_DFL;
				} 

				sigaction(SIGINT, &SIGINT_action, NULL);

				if (curr_command->is_bg) {
					// Background command uses /dev/null for input only when input redirection isn't specified in command.
					if (!curr_command->input_file) {
						int fd_in = open("/dev/null", O_RDONLY);
						dup2(fd_in, 0);
						close(fd_in);
					} 
					
					// Background command uses /dev/null for output only when output redirection isn't specified in command.
					if (!curr_command->output_file) {
						int fd_out = open("/dev/null", O_WRONLY);
						dup2(fd_out, 1);
						close(fd_out);
					}
				}

				// Handles input redirection
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

				// Handles output redirection
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
					fprintf(stderr, "%s: no such file or directory\n", curr_command->argv[0]);
					exit(1);
			} else {
				// Parent process
				if (!curr_command->is_bg) {

					int child_status;
					waitpid(child_pid, &child_status, 0);
					
					last_status = child_status;
					if (WIFSIGNALED(child_status)) {
						printf("terminated by signal %d\n", WTERMSIG(child_status));
						fflush(stdout);
					}
				} else {
						printf("background pid is %d\n", child_pid);
						fflush(stdout);
				}
			}
		}	
		free_command(curr_command);
	}
	return EXIT_SUCCESS;
}