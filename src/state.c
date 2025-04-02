#include "state.h"
#include "utils.h"
#include "worker.h"
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void reset_pollfd(struct pollfd *pollfd);

int app_init(app_state_t *state, size_t max_clients, int *err)
{
    state->npollfds    = 0;
    state->nworkers    = 0;
    state->max_clients = max_clients;

    // +1 because we want include the server socket without impacting client limits.
    errno          = 0;
    state->pollfds = (struct pollfd *)calloc(max_clients + 1, sizeof(struct pollfd));
    if(state->pollfds == NULL)
    {
        seterr(errno);
        return -2;
    }

    errno          = 0;
    state->workers = (worker_t *)calloc(max_clients, sizeof(worker_t));
    if(state->workers == NULL)
    {
        seterr(errno);
        return -3;
    }

    for(size_t idx = 0; idx < max_clients; idx++)
    {
        reset_worker(&state->workers[idx]);
    }

    for(size_t idx = 0; idx < max_clients + 1; idx++)
    {
        reset_pollfd(&state->pollfds[idx]);
    }

    return 0;
}

void app_destroy(app_state_t *state)
{
    free(state->workers);
    free(state->pollfds);
}

worker_t *app_add_worker(app_state_t *state, const worker_t *worker)
{
    memcpy(&state->workers[state->nworkers], worker, sizeof(worker_t));
    state->nworkers++;

    return &state->workers[state->nworkers - 1];
}

worker_t *app_find_available_worker(const app_state_t *state)
{
    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        worker_t *worker = &state->workers[idx];

        if(worker->pid > 0 && worker->fd > -1 && worker->client.fd == -1)
        {
            return worker;
        }
    }

    return NULL;
}

worker_t *app_find_worker_by_fd(const app_state_t *state, int fd)
{
    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        worker_t *worker = &state->workers[idx];

        if(worker->fd == fd)
        {
            return worker;
        }
    }

    return NULL;
}

worker_t *app_find_worker_by_client_fd(const app_state_t *state, int fd)
{
    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        worker_t *worker = &state->workers[idx];

        if(worker->client.fd == fd)
        {
            return worker;
        }
    }

    return NULL;
}

void app_remove_worker(app_state_t *state, pid_t pid)
{
    bool should_shift_workers = false;

    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        bool is_last_element = idx == state->max_clients;

        // Reset the pollfd obj
        if(state->workers[idx].pid == pid)
        {
            app_unpoll(state, state->workers[idx].fd);
            stop_worker(&state->workers[idx]);
            state->nworkers--;
            should_shift_workers = true;
        }

        // Shift the remaining pollfds one index down
        if(!is_last_element && should_shift_workers)
        {
            memmove(&state->workers[idx], &state->workers[idx + 1], sizeof(worker_t));    // Copy bytes from the next index over
            reset_worker(&state->workers[idx + 1]);                                       // Reset the worker -- This way, the last element wont have duplicate data
        }
    }
}

struct pollfd *app_poll(app_state_t *state, int fd)
{
    struct pollfd pollfd;

    pollfd.fd     = fd;
    pollfd.events = POLLIN | POLLHUP | POLLERR;

    memcpy(&state->pollfds[state->npollfds], &pollfd, sizeof(struct pollfd));
    state->npollfds++;

    return &state->pollfds[state->npollfds - 1];
}

void app_unpoll(app_state_t *state, int fd)
{
    bool should_shift_pollfds = false;

    for(size_t idx = 0; idx < (state->npollfds + 1); idx++)
    {
        bool is_last_element = idx == state->max_clients;

        // Reset the pollfd obj
        if(state->pollfds[idx].fd == fd && !should_shift_pollfds)
        {
            reset_pollfd(&state->pollfds[idx]);
            state->npollfds--;
            should_shift_pollfds = true;
        }

        // Shift the remaining pollfds one index down
        if(!is_last_element && should_shift_pollfds)
        {
            memmove(&state->pollfds[idx], &state->pollfds[idx + 1], sizeof(struct pollfd));    // Copy bytes from the next index over
            reset_pollfd(&state->pollfds[idx + 1]);                                            // Reset the pollfd -- This way, the last element wont have duplicate data
        }
    }
}

static void reset_pollfd(struct pollfd *pollfd)
{
    if(pollfd != NULL)
    {
        pollfd->fd      = -1;
        pollfd->events  = 0;
        pollfd->revents = 0;
    }
}
