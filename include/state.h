#ifndef STATE_H
#define STATE_H

#include "networking.h"
#include <poll.h>
#include <stdlib.h>

typedef struct
{
    size_t max_clients;

    size_t nclients;
    size_t npollfds;

    client_t      *clients;
    struct pollfd *pollfds;
} app_state_t;

int       app_init(app_state_t *state, size_t max_clients, int *err);
void      app_add_client(app_state_t *state, const client_t *client);
client_t *app_find_client(const app_state_t *state, int fd);
void      app_remove_client(app_state_t *state, int fd);

#endif
