// cppcheck-suppress-file unusedStructMember

#ifndef STATE_H
#define STATE_H

#define NUM_WORKERS 3

#include "ndbm/database.h"
#include "worker.h"
#include <poll.h>
#include <stdlib.h>

typedef struct
{
    DBM *db;

    size_t max_clients;

    size_t npollfds;
    size_t nworkers;
    size_t desired_workers;

    struct pollfd *pollfds;
    worker_t      *workers;
} app_state_t;

int app_init(app_state_t *state, size_t max_clients, int *err);
int app_destroy(app_state_t *state, int *err);

worker_t *app_create_worker(app_state_t *state, int *err);
worker_t *app_add_worker(app_state_t *state, const worker_t *worker, int *err);
worker_t *app_find_available_worker(const app_state_t *state, int *err);
int       app_remove_worker(app_state_t *state, pid_t pid, int *err);

// Worker Scaling
int app_set_desired_workers(app_state_t *state, size_t desired, int *err);
int app_health_check_workers(app_state_t *state, int *err);
int app_scale_workers(app_state_t *state, const char *public_dir, int *err);

worker_t *app_find_worker_by_fd(const app_state_t *state, int fd);
worker_t *app_find_worker_by_client_fd(const app_state_t *state, int fd);

struct pollfd *app_poll(app_state_t *state, int fd, int *err);
int            app_unpoll(app_state_t *state, int fd, int *err);

#endif
