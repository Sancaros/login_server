
/* functions */

void send_to_server(int sock, char* packet);
int receive_from_server(int sock, char* packet);
void debug(char *fmt, ...);
void debug_perror(char * msg);
void tcp_listen(int sockfd);
int tcp_accept(int sockfd, struct sockaddr *client_addr, int *addr_len);
int tcp_sock_connect(char* dest_addr, int port);
int tcp_sock_open(struct in_addr ip, int port);
