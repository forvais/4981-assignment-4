#include "state.h"
#include "logger.h"
#include "ndbm/database.h"
#include "utils.h"
#include "worker.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int reset_pollfd(struct pollfd *pollfd, int *err);

int app_init(app_state_t *state, size_t max_clients, int *err)
{
    seterr(0);
    if(state == NULL || max_clients < 1)
    {
        seterr(EINVAL);
        return -1;
    }

    if(db_init(&state->db, DB_RECORDS, err) < 0)
    {
        return -2;
    }

    state->npollfds    = 0;
    state->nworkers    = 0;
    state->max_clients = max_clients;

    // +1 because we want include the server socket without impacting client limits.
    errno          = 0;
    state->pollfds = (struct pollfd *)calloc(max_clients + 1, sizeof(struct pollfd));
    if(state->pollfds == NULL)
    {
        seterr(errno);
        return -3;
    }

    errno          = 0;
    state->workers = (worker_t *)calloc(max_clients, sizeof(worker_t));
    if(state->workers == NULL)
    {
        seterr(errno);
        return -4;    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    }

    for(size_t idx = 0; idx < max_clients; idx++)
    {
        reset_worker(&state->workers[idx], NULL);
    }

    for(size_t idx = 0; idx < max_clients + 1; idx++)
    {
        reset_pollfd(&state->pollfds[idx], NULL);
    }

    return 0;
}

int app_destroy(app_state_t *state, int *err)
{
    seterr(0);
    if(state == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    free(state->workers);
    free(state->pollfds);

    db_destroy(&state->db);

    return 0;
}

worker_t *app_create_worker(app_state_t *state, int *err)
{
    pid_t     pid;
    worker_t  worker;
    worker_t *worker_ptr;

    // Fork a worker process and setup a domain socket for communication
    // NOTE: `spawn_worker` will momentarily block until the domain socket is setup.
    seterr(0);
    pid = spawn_worker(&worker, err);
    if(pid < 0)
    {
        log_error("app_create_worker::spawn_worker: %s\n", strerror(*err));
    }

    // Add new worker to workers list
    seterr(0);
    worker_ptr = app_add_worker(state, &worker, err);
    if(worker_ptr == NULL)
    {
        // If we can't add the worker to the worker list, we don't want it the process to [essentially] be leaked, we have to clean it up.
        // We should get the worker to shutdown gracefully, if not, force-kill it with SIGKILL.

        log_error("app_create_worker::app_add_worker: Failed to add worker.\n");
        if(signal_worker(&worker, SIGINT, NULL) < 0)    // Attempt to gracefully shutdown
        {
            log_warn("app_create_worker::signal_worker: Failed to gracefully stop worker, using SIGKILL instead [PID:%d].\n", worker.pid);
            signal_worker(&worker, SIGKILL, NULL);    // Force-kill with SIGKILL
        }
    }

    if(pid != 0)
    {
        // Add worker domain socket to pollfd list
        seterr(0);
        if(app_poll(state, worker.fd, err) == NULL)
        {
            // This case occurs when a worker (that has been spawned and is tracked in the worker list) has an fd
            // that needs to be added the pollfd is unable to, for any reason.
            //
            // The consequence is that the worker will send a request that will never be processed.
            //
            // In this case, we should get the worker to quit so we can reclaim resources by following the same procedure as the `app_add_worker` error
            // but additionally, clear the entry in the worker list.

            log_error("app_create_worker::app_poll: Failed to add worker socket to pollfds list [FD:%d].\n", worker.fd);

            if(signal_worker(&worker, SIGINT, NULL) < 0)    // Attempt to gracefully shutdown
            {
                log_warn("app_create_worker::signal_worker: Failed to gracefully stop worker, using SIGKILL instead [PID:%d].\n", worker.pid);
                signal_worker(&worker, SIGKILL, NULL);    // Force-kill with SIGKILL
            }

            if(reset_worker(&worker, NULL) < 0)
            {
                // Something is seriously wrong at this point...
                log_error("app_create_worker::reset_worker: Failed to reset worker.\n");
                return NULL;
            }
        }
    }

    return worker_ptr;
}

/*
 * Add a new worker to the end of the workers list.
 */
worker_t *app_add_worker(app_state_t *state, const worker_t *worker, int *err)
{
    seterr(0);
    if(state == NULL || worker == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

    memcpy(&state->workers[state->nworkers], worker, sizeof(worker_t));
    state->nworkers++;

    return &state->workers[state->nworkers - 1];
}

worker_t *app_find_available_worker(const app_state_t *state, int *err)
{
    seterr(0);
    if(state == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

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

/*
 * Remove a new worker and shift the workers to fill the gap.
 */
int app_remove_worker(app_state_t *state, pid_t pid, int *err)
{
    bool should_shift_workers = false;

    seterr(0);
    if(state == NULL || pid <= 0)
    {
        seterr(EINVAL);
        return -1;
    }

    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        worker_t *worker          = &state->workers[idx];
        bool      is_last_element = idx == state->max_clients;

        if(worker->pid == pid)
        {
            // Remove worker domain socket from poll list
            seterr(0);
            if(app_unpoll(state, worker->fd, err) < 0)
            {
                if(err && *err == EINVAL)
                {
                    // The worker does not have a valid domain socket, likely means data corruption has occurred and this worker obj
                    // can not be trusted
                    log_error("app_remove_worker::app_unpoll: Worker has an invalid domain socket FD, skipping [PID:%d,FD:%d].\n", worker->pid, worker->fd);
                    continue;    // We use continue here so that this worker obj doesn't get overwriten and allow us to clean it up later.
                                 // Overwriting the record would be disasterous because assuming there were a worker, we would lose track
                                 // of it and it would remain forever until the next system reboot or manual clean up were done.
                }

                log_error("app_remove_worker::app_unpoll: Failed to remove worker domain socket from pollfds list [PID:%d,FD:%d].\n", worker->pid, worker->fd);
            }

            // Kill the worker
            seterr(0);
            if(signal_worker(worker, SIGINT, err) < 0)    // Attempt to gracefully shutdown
            {
                log_warn("app_remove_worker::signal_worker: Failed to gracefully stop worker, using SIGKILL instead [PID:%d].\n", worker->pid);
                signal_worker(worker, SIGKILL, NULL);    // Force-kill with SIGKILL
            }

            // Set worker back to default values
            seterr(0);
            if(reset_worker(worker, err) < 0)
            {
                log_error("app_remove_worker::reset_worker: Failed to reset worker.\n");
            }

            state->nworkers--;
            should_shift_workers = true;
        }

        // Shift the remaining workers one index down
        if(!is_last_element && should_shift_workers)
        {
            memmove(&state->workers[idx], &state->workers[idx + 1], sizeof(worker_t));    // Copy bytes from the next index over
            reset_worker(&state->workers[idx + 1], NULL);                                 // Reset the worker -- This way, the last element wont have duplicate data
        }
    }

    return 0;
}

int app_set_desired_workers(app_state_t *state, size_t desired, int *err)
{
    seterr(0);
    if(state->max_clients < desired)
    {
        seterr(ERANGE);
        return -1;
    }

    state->desired_workers = desired;
    return 0;
}

int app_health_check_workers(app_state_t *state, int *err)
{
    seterr(0);
    if(state == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    for(size_t idx = 0; idx < state->nworkers; idx++)
    {
        const worker_t *worker = &state->workers[idx];
        int             status = 0;

        if(worker->pid == 0)
        {
            continue;
        }

        if(waitpid(worker->pid, &status, WNOHANG | WUNTRACED) != 0 && status > 0 && (WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status)))
        {
            app_remove_worker(state, worker->pid, NULL);
        }
    }

    return 0;
}

int app_scale_workers(app_state_t *state, const char *public_dir, int *err)
{
    if(state->nworkers == state->desired_workers)
    {
        return 0;
    }

    // If there are less workers than desired
    if(state->nworkers < state->desired_workers)
    {
        size_t delta = state->desired_workers - state->nworkers;    // Number of workers missing

        for(size_t idx = 0; idx < delta; idx++)
        {
            const worker_t *worker;

            seterr(0);
            worker = app_create_worker(state, err);
            if(worker == NULL)
            {
                return -2;
            }

            if(worker->pid == 0)    // Worker
            {
                worker_entrypoint(state->db, public_dir);
            }
        }
    }

    // If there are more workers than desired
    if(state->nworkers > state->desired_workers)
    {
        size_t delta = state->nworkers - state->desired_workers;    // Number of extra workers

        for(size_t idx = 0; idx < delta; idx++)
        {
            const worker_t *worker;

            worker = app_find_available_worker(state, err);
            if(worker)
            {
                app_remove_worker(state, worker->pid, NULL);
            }
        }
    }

    return 0;
}

struct pollfd *app_poll(app_state_t *state, int fd, int *err)
{
    struct pollfd *pollfd;

    seterr(0);
    if(state == NULL || fd < 0)
    {
        seterr(EINVAL);
        return NULL;
    }

    pollfd         = &state->pollfds[state->npollfds];
    pollfd->fd     = fd;
    pollfd->events = POLLIN | POLLHUP | POLLERR;

    state->npollfds++;

    return pollfd;
}

int app_unpoll(app_state_t *state, int fd, int *err)
{
    bool should_shift_pollfds = false;

    seterr(0);
    if(state == NULL || fd < 0)
    {
        seterr(EINVAL);
        return -1;
    }

    for(size_t idx = 0; idx < state->npollfds; idx++)
    {
        bool is_last_element = idx == state->max_clients;

        // Reset the pollfd obj
        if(state->pollfds[idx].fd == fd && !should_shift_pollfds)
        {
            if(reset_pollfd(&state->pollfds[idx], NULL) < 0)
            {
                log_error("app_unpoll::reset_pollfd: Failed to set pollfd to default values [IDX:%d]\n", idx);
                continue;
            }
            state->npollfds--;
            should_shift_pollfds = true;
        }

        // Shift the remaining pollfds one index down
        if(!is_last_element && should_shift_pollfds)
        {
            memmove(&state->pollfds[idx], &state->pollfds[idx + 1], sizeof(struct pollfd));    // Copy bytes from the next index over

            // Reset the pollfd -- This way, the last element wont have duplicate data
            reset_pollfd(&state->pollfds[idx + 1], NULL);
        }
    }

    return 0;
}

static int reset_pollfd(struct pollfd *pollfd, int *err)
{
    seterr(0);
    if(pollfd == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    pollfd->fd      = -1;
    pollfd->events  = 0;
    pollfd->revents = 0;

    return 0;
}
