#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <unistd.h>
#include <netinet/in.h>

extern struct listen_sock l_sock;
extern struct client_sock *clients;
extern int serv_pid;

#ifndef MAX_NAME
    #define MAX_NAME 10
#endif

#ifndef MAX_BACKLOG
    #define MAX_BACKLOG 5
#endif

#ifndef MAX_USER_MSG
    #define MAX_USER_MSG 128
#endif

#ifndef MAX_PROTO_MSG
    #define MAX_PROTO_MSG MAX_NAME+1+MAX_USER_MSG+2
#endif

#ifndef BUF_SIZE
    #define BUF_SIZE MAX_PROTO_MSG+1
#endif

struct client_sock {
    int sock_fd;
    int state;
    int username;
    char buf[BUF_SIZE];
    int inbuf;
    struct client_sock *next;
};

struct listen_sock {
    struct sockaddr_in *addr;
    int sock_fd;
};

void space_remover(const char *input, char *output);

int write_buf_to_client(struct client_sock *c, char *buf, int len);
int remove_client(struct client_sock **curr, struct client_sock **clients);
int read_from_client(struct client_sock *curr);

void setup_server_socket(struct listen_sock *s, int port_number);
int write_to_socket(int sock_fd, char *buf, int len);
int find_network_newline(const char *buf, int n);
int read_from_socket(int sock_fd, char *buf, int *inbuf);
int get_message(char **dst, char *src, int *inbuf);

void handling_exit(int sig);

int exiting_server(struct listen_sock s, struct client_sock *clients);
void terminating_server();

int accept_connection(int fd, struct client_sock **clients, int *num, int *total);

/* Type for command handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*cmd_ptr)(char **);
ssize_t cmd_start_server(char **tokens);
ssize_t cmd_close_server(char **tokens);
ssize_t cmd_send(char **tokens);
ssize_t cmd_start_client(char **tokens);

/* Return: index of command or -1 if cmd doesn't match a command
 */
cmd_ptr check_command(const char *cmd);


/* COMMANDS and COMMANDS_FN are parallel arrays of length COMMANDS_COUNT
 */
static const char * const COMMANDS[] = {"start-server", "close-server", "send", "start-client"};
static const cmd_ptr COMMANDS_FN[] = {cmd_start_server, cmd_close_server, cmd_send, cmd_start_client, NULL};    // Extra null element for 'non-command'
static const ssize_t COMMANDS_COUNT = sizeof(COMMANDS) / sizeof(char *);

#endif
