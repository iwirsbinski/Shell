#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

// sets up file for redirecting
void setup_redirect(char* file) {
        FILE *fp = fopen(file, "w");
        int fd = fileno(fp);
        dup2(fd, 1);
	dup2(fd, STDERR_FILENO);
        return;
}

// returns true (1) or false (0) if the string is a built in command
int is_built_in(char* command) {
	if (strcmp(command, "exit") == 0 || strcmp(command, "cd") == 0 || strcmp(command, "path") == 0) {
		return 1;
	} else { return 0; }
}

// returns an array of paths for each command in args.
// if no path is found for any of the commands, return null
//
char** check_access(char** paths, char*** args, int num_proc,  int num_paths) {
	char** exec_paths = malloc(sizeof(char*) * num_proc);
	for (int i = 0; i < num_proc; i++) {
		int j = 0;
		if (is_built_in(args[i][0])) { return exec_paths; } // used to be && numpaths == 0
		for (j = 0; j < num_paths; j++) {
		//	if (paths[j] == NULL) { num_paths++; continue; } // removed paths are null, skip this spot and increment num paths to hit every path
			char *ep = malloc(sizeof(paths[j]) + sizeof(args[i][0]));
                	strcpy(ep, paths[j]);
			ep = strcat(ep, "/"); // add that extra / that the user leaves out
                	char *exec_path = strcat(ep, args[i][0]);
                	strtok(exec_path, "\n");
			if (access(exec_path, X_OK) != -1 || strcmp(args[i][0], "exit") == 0 ||
			  strcmp(args[i][0], "cd") == 0 || strcmp(args[i][0], "path") == 0 ) { exec_paths[i] = exec_path; break; }
		}
		if (j == num_paths) { return NULL; }
	}
	return exec_paths;
}

// call built in exit command. if synax is wrong (exit given any args), return -1
int built_in_exit(char** args) {
	if (args[1] != NULL) { return -1; }
	exit(0);
}

// built in change directory. returns 0 on sucess and -1 on fail
int change_directory(char** args) {
	if (args[1] == NULL) { return -1; }
	if (args[2] != NULL) { return -1; }
	int rc = chdir(args[1]);
	if (rc == -1) { return -1; }
	return 0;
}

// reorganize paths array so there are no null spots after removal
// returns pointer to a duplicate of paths array with null places removed
char** remove_null(char** paths, int num_paths) {
	char** paths_copy = malloc(sizeof(char*) * num_paths);
	int removed = 0;
	for (int i = 0; i < num_paths; i++) {
		if (paths[i] == NULL) { removed++;  continue; }
		paths_copy[i - removed] = paths[i];
	}
	return paths_copy;
}
// calls built in path command
// args: args passed in to path command
// return 0 on sucess, -1 on fail
int path(char*** paths, char** args, int *num_paths) {
	char** paths_der = *paths; // we pass in a pointer to paths array, in order to edit its reference when removal happens
	if (strcmp(args[1], "add") == 0) {
	//	int i = 2;
		if (args[3] != NULL) { return -1; } // too many args!
	//	while (paths[i] != NULL) { i++; }
		if (args[2] != NULL) { paths_der[*num_paths] = args[2]; *num_paths = *num_paths + 1; }
		else { return -1; } // not enough args!!
	}
	if (strcmp(args[1], "remove") == 0) {
		if (args[2] == NULL) { return -1; }
	//	int num_paths_copy = *num_paths;
		for (int i = 0; i < *num_paths; i++) {
		//	if (paths[i] == NULL) { num_paths_copy++; continue; }
			if (strcmp(args[2], paths_der[i]) == 0) {
				paths_der[i] = NULL;
				*paths = remove_null(*paths, *num_paths);
				*num_paths = *num_paths - 1;
				return 0;
			}
		}
		return -1;
        }
	if (strcmp(args[1], "clear") == 0) {
		free(*paths);
		paths = malloc(sizeof(char**));
		*paths = malloc(sizeof(char*) * 100);
		*num_paths = 0; // TODO: can i make num_paths global so this does something?
		return 0;
	}
}

// runs the specified process in parallel
// args: 2d string array, lists the args of each command
// commands: parallel array to args, lists the path for each command
// num_proc: number of commands, should be lengh of commands and args
// returns -1 on fail, 0 on success
// TODO: need to modify so we check access for all commands at once, also need to modift check_access method
int parallel_exec(char* commands[], char*** args, int num_proc, char** redir_files, char** paths, int *num_paths) {
        int* pids = malloc(sizeof(int) * 100);;
	for (int i = 0; i < num_proc; i++) {

		char** exec_paths = check_access(paths, args, num_proc, *num_paths);
		if (exec_paths == NULL) { return -1; }
	        // check for built in command exit. if this fails, return -1
	        if (strcmp(args[i][0], "exit") == 0) {
			if (built_in_exit(args[i]) == -1) { return -1; }
			continue;
		}
		// check for built in command paths. if this fails, return -1
		if (strcmp(args[i][0], "path") == 0) {
			if (path(&paths, args[i], num_paths) == -1) { return -1; }
			continue;
		}
		if (strcmp(args[i][0], "cd") == 0) {
			if (change_directory(args[i]) == -1) { return -1; }
			continue;
		}
                // make new process
                int fork_rc = fork();
		pids[i] = fork_rc;
                // if this is child, transform it into desired process. else, loop again and run next process
                if (fork_rc == 0) {
			// check for redirection
			if (redir_files[i] != NULL) { setup_redirect(redir_files[i]); }
			// start exececuting command
                        int exec_rc = execv(exec_paths[i], args[i]); // used to be commands[i]
                        return -1;
                }
        }
        // wait for all children to finish before returning
	for (int i = 0; i < num_proc; i++) { int wait_rc = waitpid(pids[i], NULL,  0); }
        return 0;
}

void error_message() {
	char error_message[30] = "An error has occurred\n";
    	write(STDERR_FILENO, error_message, strlen(error_message));
	return;
}

//checks if char* p is whitespace (a space or a newline) returns 0 if no and 1 if yes
int is_whitespace(char* p) {
	if (p == NULL) { return 0; }
	if (strcmp(p, " ") == 0 || strcmp(p, "\n") == 0 || strcmp(p, "") == 0 || strcmp(p, "\t") == 0) { return 1; }
	else { return 0; }
}

// returns 1 if the string contains an operator, ; & or >, returns 0 else
int contains_op(char* p) {
	if (p == NULL) { return 0; }
	if (strstr(p, ";") != NULL || strstr(p, "&") != NULL || strstr(p, ">") != NULL) {
		return 1;
	}
	else { return 0; }
}

// returns 1 if this string is only an operator, else return 0
int is_op(char* p) {
	if (p == NULL) { return 0; }
	if (strcmp(p, ";") == 0 || strcmp(p, "&") == 0 || strcmp(p, ">") == 0) {
                return 1;
        }
        else { return 0; }
}

// takes in a string and seperates the commands by the operators found between them. adds the commands to myargs and continues
int seperate_operators(int* index, char* p, char** my_args, char*** args, int* num_commands, char** redir_files, char** paths, int* num_paths, char** commands) {
	int begin = 0;
	// loop through the string char by char
	int len = strlen(p);
	int i = 0;
	for (i = 0; i < len; i++) {
		if (p[i] == ';') {
			char* command = malloc(i - begin);
			strncpy(command, p + begin, i - begin);
			my_args[*index] = command;
			args[*num_commands] = my_args;
			*num_commands = *num_commands + 1;
			begin = i + 1;
			if (parallel_exec(commands, args, *num_commands, redir_files, paths, num_paths) == - 1) { return -1; }
			free(args);
			args = malloc(sizeof(char**) * 100);
			*num_commands = -1;
			return 0;

		}
		if (p[i] == '&') {
			char* command = malloc(i - begin);
                        strncpy(command, p + begin, i - begin);
                        my_args[*index] = command;
                        args[*num_commands] = my_args;
                        *num_commands = *num_commands + 1;
                        begin = i + 1;
                       // if (parallel_exec(commands, args, *num_commands, redir_files, paths, num_paths) == - 1) { return -1; }
                       // free(args);
                       // args = malloc(sizeof(char**) * 100);
                       // *num_commands = -1;
                        return 0;
                }
		if (p[i] == '>') {
		}
	}
	// if being != index + 1,
}

int main(int argc, char **argv) {

	// set up search paths
	char* default_path = "/bin/";
	char **paths = malloc(sizeof(char*) * 100);
	paths[0] = "/bin/";
	int num_paths = 1;
	FILE *in;
	int batch = 0;
	//interactive mode
	if (argc == 1) { in = stdin; }

	// batch mode
	if (argc == 2) {
		batch = 1;
		in = fopen(argv[1], "r");
		if (in == NULL) { error_message(); exit(1); }
	}
	if (argc > 2) { error_message(); exit(1); }

	size_t bufsize = 32;
	char *read_line = malloc(bufsize);
	// get first line of inputi
	if (batch == 0) { printf("smash> "); }
	fflush(stdout);
	int in_rc = getline(&read_line, &bufsize, in);

	// start loop to continue getting commands from user
	while(in_rc != -1) {
		char** commands = malloc(sizeof(char*) * 100); // holds the path for each command
		char*** args = malloc(sizeof(char**) * 100);
		char** redir_files = malloc(sizeof(char*) * 100);
		for (int i = 0; i < 100; i++) { redir_files[i] = NULL; }
		char *p = strsep(&read_line, " ");
		char *backup = p;
		int num_commands = 0;
		// loop for each seperate command
		while (p != NULL) {
			if (is_whitespace(p) == 1) { p = strsep(&read_line, " "); continue; }
			char *first_command = strdup(p);
			strtok(first_command, "\n");
			strtok(first_command, "\t");
			// build exec paths
			char *ep = malloc(sizeof(default_path) + sizeof(p));
			strcpy(ep, default_path);
			char *exec_path = strcat(ep, p);
			strtok(exec_path, "\n");
		        char** my_args = malloc(sizeof(char*) * 100);
			// if this token contains an operator (but is not just an operator) separate them
			if (contains_op(p) == 1 && is_op(p) != 1) {
				int dum = 0;
                                seperate_operators(&dum, p, my_args, args, &num_commands, redir_files, paths, &num_paths, commands);
				p = strsep(&read_line, " ");
                                continue;
                        } else {
				my_args[0] = first_command; // first is command name (used to be exec_path
				commands[num_commands] = exec_path;
			}

			// look through cml args provided (contained in read_line), add them to my_args
			int index = 1;
			int redir = 0;
			int err = 0; // 0 when all is well, -1 if error
			while (p != NULL && strcmp(p, "&") != 0 && strcmp(p, ";") != 0) {
				p = strsep(&read_line, " ");
				if (contains_op(p) == 1 && is_op(p) != 1) { 
					seperate_operators(&index, p, my_args, args, &num_commands, redir_files, paths, &num_paths, commands);
				        break;
				}
				// if the last argument was a file redirect, and something comes after it, throw error
				if (redir == 1 && !(p == NULL || strcmp(p, "&") == 0 || strcmp(p, ";") == 0 || 
						is_whitespace(p) == 1)) { err = -1; break; }
				redir = 0;
				strtok(p, "\n"); // get rid of newline character at end
				strtok(p, "\t"); // get rid of any potental tab characters attached
				if (p == NULL) { break; }
				if (is_whitespace(p) == 1) { continue; } 
				// check for each type of operator
				if (strcmp(p, ";") == 0) {
					my_args[index] = NULL;
					args[num_commands] = my_args;
					err = parallel_exec(commands, args, num_commands + 1, redir_files, paths, &num_paths);
					if (err == -1) { break; }
					// free args because those commands have been run
					free(args);
					args = malloc(sizeof(char**) * 100);
					free(redir_files);
					redir_files = malloc(sizeof(char*) * 100);
					num_commands = -1;
					index = 0;
					continue;
				}
				// add to the list of commands to be run in parallel
				if (strcmp(p, "&") == 0) {
					my_args[index] = NULL;
					args[num_commands] = my_args;
					continue;
				}
				if (strcmp(p, ">") == 0) {
					// call method to handle that shit
					redir = 1; // set this to true, so we dont add the file to the args
					p = strsep(&read_line, " ");
					while (is_whitespace(p) == 1) {
						p = strsep(&read_line, " ");
					}
					strtok(p, "\n");
					strtok(p, "\t");
					if (p == NULL) { err = -1; break; }
					redir_files[num_commands] = p;
				}
				if (redir == 0 ) {
				        my_args[index] = p; 
				        index++;
				}
			}
			if (err == -1) {
				error_message();
				break;
			}
			// set up my args
			my_args[index] = NULL;
			if (p == NULL) { args[num_commands] = my_args; }
			p = strsep(&read_line, " ");
			num_commands++;
		}

		// run any commands that we havent run yet
		if (parallel_exec(commands, args, num_commands, redir_files, paths, &num_paths) == -1) {
			error_message();
		}
		if (batch == 0) { printf("smash> "); }
		fflush(stdout);
                in_rc = getline(&read_line, &bufsize, in);
		free(backup);
	}
	free(read_line);
	exit(0);
}
