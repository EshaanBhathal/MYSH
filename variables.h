#ifndef __VARIABLES_H__
#define __VARIABLES_H__

#include <unistd.h>

typedef ssize_t (*var_ptr)(char **);

typedef struct var {
	char *name;
	char *value;
	struct var *next;
} variable_node;

void free_nodes();

ssize_t set_variable(char *tokens);
int expand_token(char *input, int max, int *total_length);

#endif

