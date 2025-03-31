#ifndef HANDLERS_H
#define HANDLERS_H

#include "state.h"
#include <poll.h>

void    handle_client_connect(int sockfd, app_state_t *app);
void    handle_client_disconnect(int connfd, app_state_t *app);
ssize_t handle_client_data(int connfd);

#endif
