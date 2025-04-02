#ifndef HANDLERS_H
#define HANDLERS_H

#include "state.h"
#include <poll.h>
#include <unistd.h>

void    handle_client_connect(int sockfd, app_state_t *app);
ssize_t handle_client_data(int connfd);

ssize_t handle_workers(worker_t *workers, size_t nworkers, app_state_t *app);
ssize_t handle_worker_connect(const worker_t *worker, int fd);
ssize_t handle_worker_disconnect(worker_t *worker, app_state_t *app);

#endif
