// cppcheck-suppress-file unusedStructMember

#ifndef STATE_H
#define STATE_H

#include "worker.h"
#include <poll.h>
#include <stdlib.h>

typedef struct
{
    size_t max_clients;

    size_t npollfds;
    size_t nworkers;

    struct pollfd *pollfds;
    worker_t      *workers;
} app_state_t;

int  app_init(app_state_t *state, size_t max_clients, int *err);
void app_destroy(app_state_t *state);

worker_t *app_add_worker(app_state_t *state, const worker_t *worker);
worker_t *app_find_available_worker(const app_state_t *state);
worker_t *app_find_worker_by_fd(const app_state_t *state, int fd);
worker_t *app_find_worker_by_client_fd(const app_state_t *state, int fd);
void      app_remove_worker(app_state_t *state, pid_t pid);

struct pollfd *app_poll(app_state_t *state, int fd);
void           app_unpoll(app_state_t *state, int fd);

#endif
