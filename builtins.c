#include <string.h>

#include "builtins.h"
#include "io_helpers.h"
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h> 
#define CUST_PATH_MAX 1000
#define CUST_LINE_MAX 1000

int jobs = 0;
pid_t pids[1000];
char *job_commands[1000];

// ====== Command execution =====

/* Return: index of builtin or null if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;

    if (tokens[index] != NULL) {
        // Implement the echo command
	display_message(tokens[index]);
	index += 1;
    }
    while (tokens[index] != NULL) {
        // Implement the echo command
	display_message(" ");
        display_message(tokens[index]);
	index += 1;
    }
    display_message("\n");

    return 0;
}

/* Prereq: path is a NULL terminated string.
 * Return 0 on success and -1 on error.
 */
ssize_t recursive_ls(char *path, long int depth, int has_f, char *f_string) {

        // access the directory
        DIR *directory = opendir(path);
    if (directory == NULL) {
        display_error("ERROR: Invalid path", "");
        return -1;
    }

    char line[CUST_LINE_MAX];
    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {

	if (!(has_f == 1 && strstr(entry->d_name, f_string) == NULL)) {
                sprintf(line, "%s\n", entry->d_name);
                display_message(line);
        }

        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subpath[CUST_LINE_MAX];
            sprintf(subpath, "%s/%s", path, entry->d_name);

            if (depth == -1) {
                recursive_ls(subpath, -1, has_f, f_string);
            } else if (depth > 1) {
                recursive_ls(subpath, depth-1, has_f, f_string);
            }
        }
    }

        closedir(directory);
        return 0;
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error.
 */
ssize_t bn_ls(char **tokens) {
	
	// handle any issues with flags
	int has_f = 0;
	char f_string[MAX_STR_LEN+1] = "";
	int has_rec = 0;
	int has_d = 0;
	long int depth = 0;
	int k = 1;
	char *path = "";
	int has_path = 0;

	// check the flags
	while(tokens[k] != NULL) {
		char *temp = tokens[k];
		
		if (strcmp(temp, "--f") == 0) {
			
			if (has_f == 1) {
				k++;
				if (tokens[k] == NULL) {
					display_error("ERROR: No substring given after the flag --f", "");
					return -1;
				} else if (strcmp(tokens[k],f_string) != 0) {
					display_error("ERROR: Multiple substrings given after the flag --f", "");
					return -1;
				}
			} else {
				k++;
                                if (tokens[k] == NULL) {
                                        display_error("ERROR: No substring given after the flag --f", "");
					return -1;
                                }
				strncpy(f_string,tokens[k],MAX_STR_LEN);
				f_string[strlen(tokens[k])] = '\0';
			}
			has_f = 1;

		} else if (strcmp(temp, "--rec") == 0) {
			
			has_rec = 1;

		} else if (strcmp(temp, "--d") == 0) {
			
			char *ptr;
			if (has_d == 1) {
                                k++;
                                if (tokens[k] == NULL) {
                                        display_error("ERROR: No integer given after the flag --d", "");
					return -1;
                                }
				
				long int temp_num = strtol(tokens[k], &ptr, 10);
				if (ptr == tokens[k]){
					display_error("ERROR: Not an integer after the flag --d", "");
					return -1;
				}
				
				if (temp_num != depth) {
                                        display_error("ERROR: Multiple depths given after the flag --d", "");
					return -1;
                                }
                        } else {
                      		k++;
                                if (tokens[k] == NULL) {
                                        display_error("ERROR: No integer given after the flag --d", "");
					return -1;
                                }
				depth = strtol(tokens[k], &ptr, 10);
				if (ptr == tokens[k]){
                                        display_error("ERROR: Not an integer after the flag --d", "");
					return -1;
                                }
			}
			has_d = 1;

		} else if ((has_path == 0) || (has_path == 1 && strcmp(path, temp) == 0)) {
          			path = temp;
          			has_path = 1;
        } else if (has_path == 1 && strcmp(path, temp) != 0) {
            display_error("ERROR: Too many arguments: ls takes a single path", "");
            return -1;
	} else {
			display_error("ERROR: Invalid argument", "");
			return -1;
		}

		k++;
	}

	if (has_d == 1 && has_rec == 0) {
		display_error("ERROR: the flag --d was given without --rec", "");
		return -1;
	}

    if (has_path == 0) {
	path = "./";
    }

    // access the directory
    DIR *directory = opendir(path);
    if (directory == NULL) {
          display_error("ERROR: Invalid path", "");
          return -1;
    }

    char line[CUST_LINE_MAX];
    struct dirent *entry;

    while ((entry = readdir(directory)) != NULL) {

        if (entry->d_name[0] == '.' && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            continue;
        }
	
	if (!(has_f == 1 && strstr(entry->d_name, f_string) == NULL)) {
        	sprintf(line, "%s\n", entry->d_name);
        	display_message(line);
	}

        if (has_rec == 1 && entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char subpath[CUST_LINE_MAX];
            sprintf(subpath, "%s/%s", path, entry->d_name);

            if (has_d == 0) {
                recursive_ls(subpath, -1, has_f, f_string);
            } else if (depth > 1) {
                recursive_ls(subpath, depth-1, has_f, f_string);
            }
        }
    }

	closedir(directory);
	return 0;
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error.
 */
ssize_t bn_wc(char **tokens) {

	FILE *file;
	int no_file = 0;

   	// error checking
	if (tokens[1] == NULL) {
		no_file = 1;
	}
	if (tokens[2] != NULL) {
		display_error("ERROR: Too many arguments: wc takes a single file", "");
		return -1;
	}

	// getting the file path
        ssize_t word_count = 0;
	ssize_t character_count = 0;
	ssize_t newline_count = 0;

	char *path = tokens[1];

	//opening the file

	if (no_file == 1) {
        	file = stdin;
	} else {
		file = fopen(path, "r");
        	if (file == NULL) {
                	display_error("ERROR: Cannot open file", "");
                	return -1;
        	}
	}

	char ch;
    	int in_word = 0;

	while ((ch = fgetc(file)) != EOF) {
        	character_count++;

        	if (ch == '\n') {
            		newline_count++;
        	}

        	if (isalnum(ch)) {
            		if (0 == in_word) {
                		word_count++;
                		in_word = 1;
            		}
        	} else {
            		in_word = 0;
        	}
    	}

	char line1[MAX_STR_LEN + 1];
	char line2[MAX_STR_LEN + 1];
	char line3[MAX_STR_LEN + 1];

	sprintf(line1, "word count %ld\n", word_count);
	sprintf(line2, "character count %ld\n", character_count);
	sprintf(line3, "newline count %ld\n", newline_count);

	display_message(line1);
	display_message(line2);
	display_message(line3);

	if (no_file == 0) {
		fclose(file);
	} else {
		clearerr(stdin);
	}
	return 0;
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error.
 */
ssize_t bn_cat(char **tokens) {

	FILE *file;
        int no_file = 0;

        // error checking
        if (tokens[1] == NULL) {
        	no_file = 1;
	}
        if (tokens[2] != NULL) {
                display_error("ERROR: Too many arguments: cat takes a single file", "");
                return -1;
        }

        char *path = tokens[1];

        //opening the file
	if (no_file == 1) {
                file = stdin;
        } else {
                file = fopen(path, "r");
                if (file == NULL) {
                        display_error("ERROR: Cannot open file", "");
                        return -1;
                }
        }

	char buffer[CUST_LINE_MAX];

        while ((fgets(buffer, sizeof(buffer), file)) != NULL) {
        	display_message(buffer);
	}

	if (no_file == 0) {
                fclose(file);
        } else {
		clearerr(stdin);
	}
        return 0;
}

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error.
 */
ssize_t bn_cd(char **tokens) {

        // error checking
        if (tokens[1] == NULL) {
                display_error("ERROR: No path provided","");
                return -1;
        }
        if (tokens[2] != NULL) {
                display_error("ERROR: Too many arguments: cd takes a single path", "");
                return -1;
        }

	// fixing the path
        char *path = tokens[1];
	char expanded_path[CUST_PATH_MAX] = "";
	char temp_string[CUST_LINE_MAX] = "";

	int temp_size = 0;
	for (size_t i = 0; i < strlen(path); i++){
		
		// for absolute path case
		if (i == 0 && path[i] == '/') {
		     	expanded_path[0] = '/';
			continue;
		}

		if (path[i] == '/') {
			temp_string[temp_size] = '/';
			temp_size++;
			temp_string[temp_size] = '\0';
			
			if (strcmp(temp_string, ".../") == 0) {
				strcpy(temp_string, "../../");
				temp_string[6] = '\0';
			} else if (strcmp(temp_string, "..../") == 0) {
				strcpy(temp_string, "../../../");
                                temp_string[9] = '\0';
			}

			strcat(expanded_path, temp_string);
			temp_size = 0;
			temp_string[0] = '\0';

		} else if (i == strlen(path) - 1) {
                        temp_string[temp_size] = path[i];
			temp_size++;
			temp_string[temp_size] = '\0';

                        if (strcmp(temp_string, "...") == 0) {
                                strcpy(temp_string, "../..");
                                temp_string[5] = '\0';
                        } else if (strcmp(temp_string, "....") == 0) {
                                strcpy(temp_string, "../../..");
                                temp_string[8] = '\0';
                        }

                        strcat(expanded_path, temp_string);
			temp_size = 0;
                        temp_string[0] = '\0';
                } else {
			temp_string[temp_size] = path[i];
			temp_size++;
		}

	}

        // changing the directory
        if (chdir(expanded_path) == -1) {
                display_error("ERROR: Invalid path", "");
                return -1;
        }

	return 0;
}

ssize_t bn_ps( __attribute__((unused)) char **tokens) {
    
        char message[MAX_STR_LEN];

        for (int i = 0; i < jobs; i++) {
		char temp_line[80];
		int k = 0, j = 0;
		while (job_commands[i][k] == ' ' || job_commands[i][k] == '\t') {
       	 		k++;
		}

		while (job_commands[i][k] != ' ' && job_commands[i][k] != '\n' && job_commands[i][k] != '\0') {
        		temp_line[j++] = job_commands[i][k++];
    		}
    		temp_line[j] = '\0';

                if (pids[i] != 0) {
                        sprintf(message, "%s %d\n", temp_line, pids[i]);
                        display_message(message);
                }
        }

    	return 0;
}

ssize_t bn_kill( char **tokens) {

	if (tokens[1] == NULL || tokens[3] != NULL) {
		display_error("ERROR: Incorrect number of arguments", "");
		return -1;
	}

	char *endptr;
	int num = (int) strtol(tokens[1], &endptr, 10);
	if (*endptr != '\0' && *endptr != '\n' && *endptr != ' ') {
		display_error("ERROR: The process does not exist", "");
		return -1;
	}

	int sig = SIGTERM;
    	if (tokens[2] != NULL) {
        	sig = (int)strtol(tokens[2], &endptr, 10);
        	if (*endptr != '\0' && *endptr != '\n' && *endptr != ' ') {
            		display_error("ERROR: Invalid signal specified", "");
            		return -1;
        	}
        	if (sig <= 0 || sig >= NSIG) {
            		display_error("ERROR: Invalid signal specified", "");
            		return -1;
        	}
    	}
	
	if (kill(num, sig) == -1) {
    		if (errno == ESRCH) {
        		display_error("ERROR: The process does not exist", "");
    		} else {
        		display_error("ERROR: Failed to send signal", "");
    		}
    		return -1;
	}

	return 0;
}
