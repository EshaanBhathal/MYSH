#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include "builtins.h"
#include "commands.h"
#include "variables.h"
#include "io_helpers.h"
#include <sys/wait.h>

void free_space(char **tokens) {
	size_t k = 0;
	while (tokens != NULL && tokens[k] != NULL) {
		free(tokens[k]);
		k++;
	}
}

void handle(int sig) {
	display_message("\n");
	if (sig == SIGHUP || sig == SIGTERM) {
		exit(0);	
	}

	if (sig == SIGINT) {
		display_message("mysh$ ");
		fflush(stdout);
	}
}

void handle2(__attribute__((unused)) int sig) {
    usleep(120);
    exiting_server(l_sock, clients);
    exit(0);
}

int num_removed = 0;

void check_jobs() {
	int stat = 0;
    pid_t pid;
    char message[MAX_STR_LEN];

    for (int i = 0; i < jobs; i++) {
        pid = waitpid(pids[i], &stat, WNOHANG);
        if (pid > 0) {
            sprintf(message, "[%d]+  Done %s\n", i + 1 + num_removed, job_commands[i]);
            display_message(message);
            free(job_commands[i]);
                    job_commands[i] = NULL;
            pids[i] = 0;
            for (int j = i; j < jobs - 1; j++) {
                        pids[j] = pids[j + 1];
                        job_commands[j] = job_commands[j + 1];
            }

            pids[jobs - 1] = 0;
            job_commands[jobs - 1] = NULL;
            jobs--;
            num_removed++;
            i--;
        }
        if (jobs == 0) {
            num_removed = 0;
        }
    }
    if (jobs == 0) {
        num_removed = 0;
    }
}

// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
    char *prompt = "mysh$ ";

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};

    signal(SIGALRM, handle2);
    signal(SIGINT, handle);

    while (1) {


	signal(SIGALRM, handle2);
        signal(SIGINT, handle);

        check_jobs();
	
        // Prompt and input tokenization

        // Display the prompt via the display_message function.
        display_message(prompt);

        int ret = get_input(input_buf);

        size_t token_count = tokenize_input(input_buf, token_arr);

        int line = 1;
        size_t last = 1;
        int not_included = 0;
        int a = 0;
        size_t temp_count = token_count;

        for (size_t k = 0; k < temp_count; k++) {

                if (not_included == 1) {
                        free(token_arr[k]);
                        token_arr[k] = NULL;
                } else {
                        a = expand_token(token_arr[k], MAX_STR_LEN, &line);
                }

                if (a != 2 && k != token_count - 1) {
                        line += 1;
                }

                if (line == MAX_STR_LEN) {
                        token_count = last;
                        line = MAX_STR_LEN;
                        not_included = 1;
                }

                if(a != 2) {
                        last++;
                }
        }

        // Clean exit
        if (ret == 0) {
                display_message("\n");
                free_space(token_arr);
                free_space(job_commands);
		        break;
        }

        if (ret != -1 && (token_count > 0 && (strncmp("exit", token_arr[0], 4) == 0 && token_arr[0][4] == '\0'))) {
                free_space(token_arr);
                free_space(job_commands);
		        break;
        }

        // Variable assignment
        if (token_count == 1 && strchr(token_arr[0], '=') != NULL) {
                set_variable(token_arr[0]);
        }

        int is_bg = 0;
        if (token_count > 0 && strcmp(token_arr[token_count - 1], "&") == 0) {
            is_bg = 1;
            free(token_arr[token_count - 1]);
            token_arr[token_count - 1] = NULL;
            token_count--;
        }

        int is_child = 0;
        char job_message[2*MAX_STR_LEN];

        	if (is_bg) {
        		pid_t pid = fork();
                        if (pid < 0) {
                                display_error("Error: Fork failed", "");
                        } else if (pid == 0) {
        			is_child = 1;
        		} else {
        			pids[jobs] = pid;
        			char prompt_cur[MAX_STR_LEN + 1] = {0};
                                for (int i = 0; token_arr[i] != NULL; i++) {
                                        strcat(prompt_cur, token_arr[i]);

                                        if (token_arr[i + 1] != NULL) {
                                                strcat(prompt_cur, " ");
                                        }
                                }
        			job_commands[jobs] = malloc((MAX_STR_LEN + 1) * sizeof(char));
        			if (job_commands[jobs] == NULL) {
            				display_error("ERROR: Malloc failed", "");
            				free_space(job_commands);
        				exit(1);
        			}

                        	strcpy(job_commands[jobs], prompt_cur);
        			sprintf(job_message, "[%d] %d\n", jobs + 1 + num_removed, pid);

                        	display_message(job_message);
        			jobs++;
        			free_space(token_arr);
                    continue;
                    }
        	}

        char *str_arr[MAX_STR_LEN] = {NULL};
        size_t count =  0;
        int is_empty = 0;
	int ind = 0;
	int invalid_pipe=0;
	int pipes_exist=0;

	char *temp_str2 = malloc(MAX_STR_LEN * sizeof(char));
	if (temp_str2 == NULL) {
        	exit(1);
        }
	temp_str2[0] = '\0';

        for (size_t tok_num = 0; (tok_num < token_count && token_arr[tok_num] != NULL); tok_num++) {
                char *temp_str = token_arr[tok_num];
		is_empty = 0;	
		for (size_t ch = 0;ch < strlen(temp_str); ch++) {
                        
			//Error for pipe at end
			if (temp_str[ch] == '|' && (tok_num == token_count - 1)&& (ch == strlen(temp_str) - 1)) {
				free(temp_str2);
				free_space(token_arr);
				free_space(str_arr);
				free_nodes();
				display_error("ERROR: Invalid pipe sequence","");
				invalid_pipe=1;
				break;
			}

			if (temp_str[ch] == '|') {
                                pipes_exist=1;
				temp_str2[ind] = '\0';
				str_arr[count] = strdup(temp_str2);
                		if (str_arr[count] == NULL) {
                    			exit(1);
                		}
				count++;
				ind=0;
				is_empty = 1;

				free(temp_str2);
				temp_str2 = malloc(MAX_STR_LEN * sizeof(char));		
                                temp_str2[0] = '\0';
				if (ch != strlen(temp_str) - 1) {
					is_empty = 0;
				}
                        } else {
				temp_str2[ind] = temp_str[ch];
				ind++;
                        }
                }
		if (invalid_pipe == 1){
			break;
		}
		if (is_empty == 0 && tok_num < token_count - 1) {
			temp_str2[ind] = ' ';
			ind++;
        	} else if (tok_num != token_count && is_empty == 1) {
			free(temp_str2);
			temp_str2 = malloc(MAX_STR_LEN * sizeof(char));
        		if (temp_str2 == NULL) {
                		exit(1);
        		}
        		temp_str2[0] = '\0';
		}
	}

	if (invalid_pipe == 1) {
                continue;
        }

	if (is_empty == 0 && ind > 0) {
		temp_str2[ind] = '\0';
		str_arr[count] = strdup(temp_str2);
        	if (str_arr[count] == NULL) {
            		exit(1);
        	}
        	count++;
	}
	free(temp_str2);

        // Error checking for pipe
	for (size_t i = 0; i < count; i++) {
        	if (str_arr[i] == NULL || str_arr[i][0] == '\0') {
                        free_space(token_arr);
			free_space(str_arr);
                        free_nodes();
			display_error("ERROR: Invalid pipe sequence","");
			invalid_pipe =1;
			break;
		}
    	}
	if (invalid_pipe == 1) {
		continue;
	}

	if (pipes_exist == 1) {

		int pipefds[(2 * count) - 2];

            	for (size_t i = 0; i < count - 1; i++) {
                	if (pipe(pipefds + (i * 2)) == -1) {
                    		display_error("ERROR: Piping failed", "");
                    		exit(1);
                	}
            	}

            	for (size_t i = 0; i < count; i++) {
                	int pid_c = fork();

                    if (pid_c == 0) {

			if (i > 0) {
                        	dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
                    	}
                    	if (i < count - 1) {
                        	dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
                    	}

                    	for (size_t j = 0; j < 2 * (count - 1); j++) {
                        	close(pipefds[j]);
                    	}

		    	for (size_t i=0; i < token_count; i++) {
				free(token_arr[i]);
				token_arr[i] = NULL;
		    	}

                        token_count = 0;
                        token_count = tokenize_input(str_arr[i], token_arr);

                        // have child handle command
                        if (token_count >= 1 && !(token_count == 1 && strchr(token_arr[0], '=') != NULL)) {
                                bn_ptr builtin_fn = check_builtin(token_arr[0]);
                                cmd_ptr command_fn = check_command(token_arr[0]);

                                if (builtin_fn != NULL) {
                                    ssize_t err = builtin_fn(token_arr);
                                    if (err == - 1) {
                                            display_error("ERROR: Builtin failed: ", token_arr[0]);
                                    }
                                } else if (command_fn != NULL) {
                                      ssize_t err = command_fn(token_arr);
                                      if (err == -1) {
                                          display_error("ERROR: Builtin failed ", token_arr[0]);
                                      }
                                } else {
                                    pid_t pid = fork();
                                    if (pid < 0) {
                                            display_error("Error: Fork failed", "");
                                    } else if (pid == 0) {
                                            char result[1000];
                                            sprintf(result, "/bin/%s", token_arr[0]);
                                            execve(result, token_arr, NULL);
                                            sprintf(result, "/usr/bin/%s", token_arr[0]);
                                            execve(result, token_arr, NULL);
                                            display_error("ERROR: Unknown command: ", token_arr[0]);
                                            exit(1);
                                    } else {
                                            wait(NULL);
                                    }
                                }

                           }
                            free_space(token_arr);
                                free_nodes();
                        free_space(str_arr);
                                exit(0);
                    }
            }

            for (size_t k = 0; k < (2 * count) - 2; k++) {
                close(pipefds[k]);
            }

            for (size_t k = 0; k < count; k++) {
                wait(NULL);
            }

	        free_space(token_arr);
            free_space(str_arr);
            if (is_child == 1) {
                break;
            }
	} else {

	// Command execution
        if (token_count >= 1 && !(token_count == 1 && strchr(token_arr[0], '=') != NULL)) {
            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            cmd_ptr command_fn = check_command(token_arr[0]);

            if (builtin_fn != NULL) {
                ssize_t err = builtin_fn(token_arr);
                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                }
            } else if (command_fn != NULL) {
                  ssize_t err = command_fn(token_arr);
                  if (err == -1) {
                      display_error("ERROR: Builtin failed ", token_arr[0]);
                  }
            } else {
                pid_t pid = fork();
                if (pid < 0) {
                        display_error("Error: Fork failed", "");
                } else if (pid == 0) {
                        char result[1000];
                        sprintf(result, "/bin/%s", token_arr[0]);
                        execve(result, token_arr, NULL);
                        sprintf(result, "/usr/bin/%s", token_arr[0]);
                        execve(result, token_arr, NULL);
                        display_error("ERROR: Unknown command: ", token_arr[0]);
                        terminating_server();
			exit(1);
                } else {
                        waitpid(pid, NULL, 0);
                }
            }

        }
        free_space(token_arr);
	free_space(str_arr);
	if (is_child == 1) {
		break;
	}
	}
    }
    free_nodes();
    handling_exit(0);
    terminating_server();
    return 0;
}
