// cppcheck-suppress-file unusedStructMember

#ifndef WORKER_H
#define WORKER_H

#include "networking.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>

typedef struct
{
    int      fd;    // FD to socket used to communicate with worker
    pid_t    pid;
    client_t client;
} worker_t;

int  spawn_worker(worker_t *worker, int *err);
void stop_worker(worker_t *worker);
void reset_worker(worker_t *worker);
int  assign_client_to_worker(worker_t *worker, const client_t *client, int *err);

void worker_entrypoint(void);

#endif
