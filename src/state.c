#include "state.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void reset_client(client_t *client);
static void reset_pollfd(struct pollfd *pollfd);

int app_init(app_state_t *state, size_t max_clients, int *err)
{
    state->nclients    = 0;
    state->npollfds    = 0;
    state->max_clients = max_clients;

    errno          = 0;
    state->clients = (client_t *)calloc(max_clients, sizeof(client_t));
    if(state->clients == NULL)
    {
        seterr(errno);
        return -1;
    }

    // +1 because we want include the server socket without impacting client limits.
    errno          = 0;
    state->pollfds = (struct pollfd *)calloc(max_clients + 1, sizeof(struct pollfd));
    if(state->pollfds == NULL)
    {
        seterr(errno);
        return -2;
    }

    for(size_t idx = 0; idx < max_clients; idx++)
    {
        reset_client(&state->clients[idx]);
    }

    for(size_t idx = 0; idx < max_clients + 1; idx++)
    {
        reset_pollfd(&state->pollfds[idx]);
    }

    return 0;
}

void app_destroy(app_state_t *state)
{
    free(state->clients);
    free(state->pollfds);
}

void app_add_client(app_state_t *state, const client_t *client)
{
    struct pollfd pollfd;

    // Add client to client list
    memcpy(&state->clients[state->nclients], client, sizeof(client_t));
    state->nclients++;

    // Add client to pollfd list
    pollfd.fd     = client->fd;
    pollfd.events = POLLIN | POLLHUP | POLLERR;

    memcpy(&state->pollfds[state->npollfds], &pollfd, sizeof(struct pollfd));
    state->npollfds++;
}

client_t *app_find_client(const app_state_t *state, int fd)
{
    for(size_t offset = 0; offset < state->nclients; offset++)
    {
        // Remove the client obj from state->clients
        if(state->clients[offset].fd == fd)
        {
            return state->clients + offset;
        }
    }

    return NULL;
}

void app_remove_client(app_state_t *state, int fd)
{
    bool should_shift_clients = false;
    bool should_shift_pollfds = false;

    for(size_t idx = 0; idx < state->nclients; idx++)
    {
        bool is_last_element = idx == (state->max_clients - 1);

        // Reset the client obj
        if(state->clients[idx].fd == fd && !should_shift_clients)
        {
            reset_client(&state->clients[idx]);
            state->nclients--;
            should_shift_clients = true;
        }

        // Shift the remaining clients one index down
        if(!is_last_element && should_shift_clients)
        {
            memmove(&state->clients[idx], &state->clients[idx + 1], sizeof(client_t));    // Copy bytes from the next index over
            reset_client(&state->clients[idx + 1]);                                       // Reset the client element -- This way, the last element wont have duplicate data
        }
    }

    for(size_t idx = 0; idx < state->npollfds; idx++)
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

    close(fd);
}

static void reset_client(client_t *client)
{
    if(client != NULL)
    {
        client->fd      = -1;
        client->port    = 0;
        client->address = NULL;
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
