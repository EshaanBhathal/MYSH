
#include "variables.h"
#include "io_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static size_t var_amount = 0;
static variable_node *first = NULL;

void free_nodes() {
	
	variable_node *current = first;
	while (current != NULL) {
        	variable_node *temp = current;
        	current = current->next;

        if (temp->name != NULL) {
            free(temp->name);
            temp->name = NULL;
        }
        if (temp->value != NULL) {
            free(temp->value);
            temp->value = NULL;
        }
        free(temp);
    }

    first = NULL;
    var_amount = 0;
}

/* Prereq: tokens is a NULL terminated string
 * Return 0 on success and -1 on error.
 */
ssize_t set_variable(char *tokens) {
	
    if (tokens[0] == '=') {
        display_error("ERROR: Variable name not defined", "");
        return -1;
    }

    char *equal_sign = strchr(tokens, '=');
    if (equal_sign == NULL) {
        display_error("ERROR: Invalid input format", "");
        return -1;
    }

    size_t name_size = equal_sign - tokens;
    char var_name[name_size + 1];
    strncpy(var_name, tokens, name_size);
    var_name[name_size] = '\0';

    char *var_value = equal_sign + 1;

    variable_node *current = first;
    while (current != NULL) {
        if (strcmp(var_name, current->name) == 0) {
            
	    free(current->value);

            current->value = malloc(strlen(var_value) + 1);
            if (current->value == NULL) {
                return -1;
            }

            strcpy(current->value, var_value);
            return 0;
        }
        current = current->next;
    }

    variable_node *new_node = malloc(sizeof(variable_node));
    if (new_node == NULL) {
        return -1;
    }

    new_node->name = malloc(strlen(var_name) + 1);
    new_node->value = malloc(strlen(var_value) + 1);
    if (new_node->name == NULL || new_node->value == NULL) {
        free(new_node);
        return -1;
    }

    strcpy(new_node->name, var_name);
    strcpy(new_node->value, var_value);
    new_node->next = NULL;

    if (first == NULL) {
        first = new_node;
    } else {
        current = first;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }

    var_amount += 1;
    return 0;

}

/*
 * Return the value of the variable and null if it does not exist
 */
variable_node *get_node(char *variable, int *size) {

	if (var_amount != 0) {

		variable_node *current = first;
		for (size_t i=0; i < var_amount; i++) {
			
			if (strcmp(current->name, variable) == 0) {
				*size = strlen(current->value);
				return current; 
			}
                        current = current->next;
                }
	}
	*size = 0;
	return NULL;
}


/* Prereq: input is a NULL terminated string
 * Return expanded string and limit to max characters.
 */
int expand_token(char *input, int max, int *total_length) {

	  char new_token[max + 1];
          int length = strlen(input);
          int valid_vars = 0;
          int variable_index;
          int index = 0;
          int var_size = 0;

          int one_var = (input[0] == '$');
		
	  if (strcmp(input, "$") == 0) {
		(*total_length)++;
		return 0;
	  }

          for (int i = 0; i < length; i++) {

                  if (input[i] == '$') {
                          int start_var = i;
                          i++;

                          char name[max];
                          variable_index=0;

                          while (i < length  && input[i] != '$') {
                                  if (variable_index < max) {
                                          name[variable_index] = input[i];
                                          variable_index++;
                                  }
                                  i++;
                          }
                          name[variable_index] = '\0';

                          if (variable_index == 0) {
                                  if (index >= max) {
                                          continue;
                                  }
                                  new_token[index] = '$';
                                  index++;
                          }

                          variable_node *var = get_node(name, &var_size);

                          if (var != NULL) {
                                  int space = MAX_STR_LEN - index;
                                  if (var_size <= space) {
                                          strncpy(new_token + index, var->value, var_size);
                                          index += var_size;
                                  } else {
                                          strncpy(new_token + index, var->value, space);
                                          index += space;
                                  }

                                  valid_vars = 1;
                                  one_var = 0;

                          } else {

                                  if ( one_var && i == length ) {
                                          input[0] = '\0';
                                          return 0;
                                  } else if (valid_vars != 1) {
                                          int space = max - index;

                                          int temp_len;
                                          if (i - start_var < space) {
                                                  temp_len = i - start_var;
                                          } else {
                                                  temp_len = space;
                                          }

                                          strncpy(new_token + index, input + start_var, temp_len);
                                          index += temp_len;

                                  }
                          }
                          i--;
          } else {

                  if (index < max) {
                          new_token[index++] = input[i];
                  }
                  one_var = 0;
          }
       }

       new_token[index] = '\0';
       
       int temp_num = max - *total_length;
       *total_length += strlen(new_token);
       if (*total_length >= max) {	
	        strncpy(input, new_token, temp_num);
        	input[temp_num+1] = '\0';
        	*total_length = max;
		return 2;
	} else {
    		strncpy(input, new_token, max);
    		input[index] = '\0';
	}

       	return 1;	

}
