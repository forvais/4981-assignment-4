#ifndef HANDLERS_H
#define HANDLERS_H

#include <poll.h>
#include <stdlib.h>

void    handle_client_connect(int sockfd, struct pollfd *pollfds, size_t max_clients);
void    handle_client_disconnect(int connfd, struct pollfd *pollfds, size_t max_clients);
ssize_t handle_client_data(int connfd);

#endif
