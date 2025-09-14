#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"

struct listen_sock l_sock;
struct client_sock *clients = NULL;
int serv_pid = 0;
int current_port = 0;

int sigint_given = 0;
int c_amount = 0;
int total_c = 0;
int client_handler_triggered = 0;

// ====== Signal Handlers ======

void client_handler(__attribute__((unused)) int sig) {
    client_handler_triggered = 1;
}


// ====== Command execution =====

/* Return: index of command or null if cmd doesn't match a command
 */
cmd_ptr check_command(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < COMMANDS_COUNT &&
           strncmp(COMMANDS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return COMMANDS_FN[cmd_num];
}

// ===== Commands =====

ssize_t cmd_start_server(char **tokens) {

    int fd[2];

    if (pipe(fd) == -1) {
            display_error("Piping failed", "");
            return -1;
    }

    pid_t num = fork();

    if (num < 0) {
        display_error("ERROR: fork failed", "");
        return -1;
    }
    if (num > 0) {
        close(fd[1]);
        int rec_val= 0;
        read(fd[0], &rec_val, sizeof(rec_val));
        close(fd[0]);
        if (rec_val != 0) {
            serv_pid = num;
            current_port = rec_val;
        }
	    return 0;
    }
    else if (num == 0){
            close(fd[0]);
            if (tokens[1] == NULL) {
                    display_error("ERROR: No port provided", "");
                    return -1;
            }

            if (tokens[2] != NULL) {
                    display_error("ERROR: Too many arguments", "");
                    return -1;
            }

            signal(SIGTERM, handling_exit);
            char *prompt;
            long num2 = strtol(tokens[1], &prompt, 10);
            if (*prompt != '\0'){
                    display_error("ERROR: No port exists", "");
                    close(fd[1]);
                    return -1;
            }
            setup_server_socket(&l_sock, (int)num2);
            write(fd[1], &num2, sizeof(num2));
            close(fd[1]);

            int limit = l_sock.sock_fd;
            fd_set fdset;
            FD_ZERO(&fdset);
            FD_SET(l_sock.sock_fd, &fdset);

            fd_set basket;
            basket = fdset;

            while (1) {
		
		fdset = basket;
		
                if (select(limit + 1, &fdset, NULL, NULL, NULL) < 0) {
                    exit(1);
                }

                if (FD_ISSET(l_sock.sock_fd, &fdset)){
                    int fd_accept = accept_connection(l_sock.sock_fd, &clients, &c_amount, &total_c);

                    if (0 <= fd_accept && fd_accept > limit) {
                        limit = fd_accept;
                    }
                    if (0 > fd_accept) {
                        continue;
                    }
                    FD_SET(fd_accept, &basket);
                }

                struct client_sock *temp_c_sock = clients;

                while (temp_c_sock != NULL) {

                    if (FD_ISSET(temp_c_sock->sock_fd, &fdset)) {


			int closed = read_from_client(temp_c_sock);
                        if (closed == -1) {
                            closed = 1;
                        }

                        char *msg_prompt;

                        while (closed == 0 && !get_message(&msg_prompt, temp_c_sock->buf, &(temp_c_sock->inbuf))) {

                            char buffer[BUF_SIZE];
                            char name[MAX_NAME];
                            sprintf(name, "%d", temp_c_sock->username);

                            int msg_l = strlen(msg_prompt);
                            if (msg_l >= 2 && msg_prompt[msg_l-1] == '\n' && msg_prompt[msg_l-2] == '\r') {
                               msg_prompt[msg_l - 2] = '\0';
                            }

                            if (strcmp(msg_prompt, "\\connected") == 0) {
                                snprintf(buffer, sizeof(buffer), "# of clients: %d\r\n", c_amount);
                            } else {
                                snprintf(buffer, sizeof(buffer), "client%s: %s\r\n", name, msg_prompt);
                            }

                            free(msg_prompt);
                            int msg_length = strlen(buffer);

                            struct client_sock *other_clients = clients;
                            while (other_clients != NULL) {

                                if (write_buf_to_client(other_clients, buffer, msg_length) == 2) {
                                    close(other_clients->sock_fd);
                                    FD_CLR(other_clients->sock_fd, &basket);
                                    c_amount--;
                                    continue;
                                }
                                other_clients = other_clients->next;
                                }
                            display_message(buffer);
                        }

                        if (closed != 1) {
                            temp_c_sock = temp_c_sock->next;
                        } else if (temp_c_sock != NULL) {
                            FD_CLR(temp_c_sock->sock_fd, &basket);
                            close(temp_c_sock->sock_fd);

                            struct client_sock *t = temp_c_sock;
                            if (t != NULL) {
                                temp_c_sock = temp_c_sock->next;
                                remove_client(&t, &clients);
                                c_amount--;
                            }
                        }
                    } else {
                        temp_c_sock = temp_c_sock->next;
                        continue;
                    }
                }
            }
    }
    return 0;
}

void handling_exit(__attribute__((unused)) int sig) {
    exiting_server(l_sock, clients);
}

ssize_t cmd_close_server( __attribute__((unused)) char **tokens) {
    if (tokens[1] != NULL) {
        display_error("ERROR: Too many arguments", "");
        return -1;
    }

    if (0 >= serv_pid) {
        display_error("ERROR: No Server exists", "");
        return -1;
    } else {
        kill(serv_pid, SIGALRM);
    }
    return 0;
}

ssize_t cmd_send(char **tokens) {

    if (tokens[1] == NULL || tokens[2] == NULL || tokens[3] == NULL) {

        if (tokens[1] == NULL) {
            display_error("ERROR: No port provided", "");
            return -1;
        }
        if (tokens[2] == NULL) {
            display_error("ERROR: No hostname provided", "");
            return -1;
        }
        if (tokens[3] == NULL) {
            display_error("ERROR: No message provided", "");
            return -1;
        }
    }

    char buf[BUF_SIZE];
    buf[0] = '\0';
    int i = 3;
    while (tokens[i] != NULL) {
        strcat(buf, tokens[i]);
        i++;
        if (tokens[i] != NULL) {
            strcat(buf, " ");
        }
    }

    char *prompt;
    int port = (int)(strtol(tokens[1], &prompt, 10));
    if (*prompt != '\0') {
        display_error("ERROR: Invalid port number", "");
        return -1;
    }

    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_fd < 0) {
        display_error("ERROR: Socket failed", "");
        return -1;
    }

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    char *hostname = tokens[2];

    if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) < 1) {
        display_error("ERROR: Invalid hostname", "");
        close(s_fd);
        return -1;
    }

    if (connect(s_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
            perror("client: connect");
            close(s_fd);
            return -1;
    }

    char final_buf[BUF_SIZE*3];
    space_remover(buf, final_buf);

    size_t k = strlen(final_buf);
    if (k + 2 < sizeof(final_buf)) {
        final_buf[k] = '\r';
        final_buf[k + 1] = '\n';
        final_buf[k + 2] = '\0';
    }

    ssize_t num = write(s_fd, final_buf, strlen(final_buf));
    if (num < 0) {
        display_error("ERROR: Message failed to send", "");
    }

    close(s_fd);
    return 0;
}

ssize_t cmd_start_client(char **tokens) {

    signal(SIGINT, client_handler);

    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    }

    if (tokens[2] == NULL) {
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    char *prompt;
    int port = (int)(strtol(tokens[1], &prompt, 10));
    if (*prompt != '\0') {
        display_error("ERROR: Invalid port number", "");
        return -1;
    }

    char *hostname = tokens[2];

    char buf[BUF_SIZE];
    int sock_fd;
    int inbuf;

	inbuf = 0;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        display_error("ERROR: Socket failed", "");
        return -1;
    }

    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) < 1) {
        display_error("ERROR: Invalid hostname", "");
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
            perror("client: connect");
            close(sock_fd);
            return -1;
    }

    fd_set basket;
    FD_ZERO(&basket);
    FD_SET(STDIN_FILENO, &basket);
    FD_SET(sock_fd, &basket);

    fd_set fdsets;
    int limit = sock_fd;

    while (1) {

        fdsets = basket;

        if (select(limit + 1, &fdsets, NULL, NULL, NULL) == -1) {
            if (client_handler_triggered == 0) {
                display_error("ERROR: Select has failed", "");
            }
            return 0;
        }

        if (FD_ISSET(STDIN_FILENO, &fdsets)) {

            char buf[BUF_SIZE + 2];
            if (fgets(buf, BUF_SIZE + 2, stdin) != NULL) {

                int len = strlen(buf);
		        if (len == BUF_SIZE + 1 && buf[BUF_SIZE] != '\n') {
		            int c = getchar();
                    while (c != '\n' && c != EOF) {
                        c = getchar();
                    }
                }

                int i = strcspn(buf, "\n");
                buf[i] = '\0';

                int temp_size = strlen(buf);
                if (MAX_STR_LEN + 1 <= temp_size) {
                    display_error("ERROR: The message exceeds the limit of 128", "");
                    continue;
                }
                size_t k = strlen(buf);
                if (k + 2 < sizeof(buf)) {
                    buf[k] = '\r';
                    buf[k + 1] = '\n';
                    buf[k + 2] = '\0';
                }

                if (write_to_socket(sock_fd, buf, strlen(buf))) {
                    if (client_handler_triggered == 0) {
                        close(sock_fd);
                        break;
                    }
                    close(sock_fd);
                    return 0;
                }
            } else {
                close(sock_fd);
                return 0;
            }
        }

        if (FD_ISSET(sock_fd, &fdsets)) {

            int ret = read_from_socket(sock_fd, buf, &(inbuf));
	        if (ret == -1) {
                if (client_handler_triggered == 1) {
                    return 0;
                }
                close(sock_fd);
                return 0;
            }
            if (ret == 1) {
                if (client_handler_triggered == 1) {
                    return 0;
                }
                close(sock_fd);
                return 0;
            }

            char *msg;
            while (get_message(&msg, buf, &(inbuf)) == 0) {

	        char *i = strchr(msg, ' ');
                if (i == NULL) {
                    free(msg);
                    if (client_handler_triggered == 1) {
                        return 0;
                    }
                }
                *i = '\0';
                char *content = i + 1;

                char final_msg[BUF_SIZE];
                snprintf(final_msg, sizeof(final_msg), "%s %s", msg, content);

                display_message(final_msg);
                free(msg);
            }
        }
    }

    close(sock_fd);
    return 0;
}

// ===== Helper Commands =====

void setup_server_socket(struct listen_sock *s, int port_number) {
    if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        perror("Malloc failed");
        exit(1);
    }

    s->addr->sin_family = AF_INET;
    s->addr->sin_port = htons(port_number);
    memset(&(s->addr->sin_zero), 0, 8);
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        display_error("ERROR: Server socket failed", "");
        exit(1);
    }

    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        display_error("ERROR: setsockopt failed", "");
        exit(1);
    }

    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        display_error("ERROR: socket is currently in use", "");
        display_error("ERROR: Builtin failed: ", "start-server");
        close(s->sock_fd);
        exit(1);
    }

    if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
        display_error("ERROR: Listen failed", "");
        close(s->sock_fd);
        exit(1);
    }
}

int find_network_newline(const char *buf, int inbuf) {
    for (int k = 0; k < inbuf - 1; k++) {
        if (buf[k + 1] == '\n' && buf[k] == '\r') {
            return k + 2;
        }
    }
    return -1;
}

int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    ssize_t read_amount = read(sock_fd, buf + *inbuf, BUF_SIZE - *inbuf);

    if (read_amount < 0) {
        perror("Read");
        return -1;
    } else if (read_amount == 0) {
        return 1;
    }

    *inbuf += read_amount;

    if (find_network_newline(buf, *inbuf) != -1) {
        return 0;
    }
    return 2;
}

int get_message(char **dst, char *src, int *inbuf) {
    int nline_i;
    if ((nline_i = find_network_newline(src, *inbuf)) == -1) {
        return 1;
    }

    if ( (*dst = malloc(nline_i + 1)) == NULL) {
        perror("Allocation failed");
        return -1;
    }

    strncpy(*dst, src, nline_i);
    (*dst)[nline_i] = '\0';

    if (0 < *inbuf - nline_i) {
        memmove(src, src + nline_i, *inbuf - nline_i);
    }
    *inbuf = *inbuf - nline_i;

    return 0;
}

int write_to_socket(int sock_fd, char *buf, int len) {
    for (int num = 0; len > num;) {
        int num2 = write(sock_fd, buf + num, len - num);
        if (0 > num2) {
            perror("Write");
            return -1;
        }
        num += num2;
    }
    return 0;
}

int accept_connection(int fd, struct client_sock **clients, int *num, int *total) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    if (client_fd < 0) {
        display_error("ERROR: Server did not accept connection", "");
        close(fd);
        exit(1);
    }

    (*total)++;
    (*num)++;

    struct client_sock *newclient = malloc(sizeof(struct client_sock));
    if (newclient == NULL) {
        perror("Malloc failed");
        close(client_fd);
        return -1;
    }

    newclient->sock_fd = client_fd;
    newclient->inbuf = 0;
    newclient->state = 0;
    newclient->username = *total;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);
    if (*clients == NULL) {
        *clients = newclient;
        (*num) = 1;
    }
    else {
        struct client_sock *curr = *clients;
         while (curr->next != NULL) {
             curr = curr->next;
         }
         curr->next = newclient;
    }

    return client_fd;
}

int write_buf_to_client(struct client_sock *c, char *buf, int len) {
    int return_val = write_to_socket(c->sock_fd, buf, len);
    if (return_val == -1) {
        return 1;
    }
    if (return_val == 0) {
        return 0;
    }
    return 2;
}

int read_from_client(struct client_sock *curr) {
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int remove_client(struct client_sock **curr, struct client_sock **clients) {

    struct client_sock *s = *clients;
    struct client_sock *p = NULL;

    if (*clients == NULL) {
        return 1;
    }
    if (*curr == NULL) {
        return 1;
    }

    while (s != NULL && s != *curr) {
        p = s;
        s = s->next;
    }

    if (s == NULL) {
        return 1;
    } else {
        if (p == NULL) {
            *clients = s->next;
        } else {
            p->next = s->next;
        }
        free(s);
        *curr = NULL;

        return 0;
    }
}

void terminating_server() {
    if (0 >= serv_pid) {
        exit(0);
    } else {
        kill(serv_pid, SIGALRM);
    }
    exit(0);
}

int exiting_server(struct listen_sock s, struct client_sock *clients) {
    struct client_sock *tmp;
    while (clients != NULL) {
        tmp = clients;
        close(tmp->sock_fd);
        clients = clients->next;
        free(tmp);
    }
    close(s.sock_fd);
    free(s.addr);
    return 0;
}

void space_remover(const char *input, char *output) {
    int count = 0;
    int p = 0;
    int size = (int)strlen(input);

    while (p < size) {
        if (input[p] == ' ') {
            if (count == 0 || output[count - 1] != ' ') {
                output[count++] = input[p];
            }
        } else {
            output[count++] = input[p];
        }
        p++;
    }
    output[count] = '\0';
}
