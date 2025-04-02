#include "handlers.h"
#include "io.h"
#include "logger.h"
#include "networking.h"
#include "state.h"
#include "utils.h"
#include "worker.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFLEN 1024

void handle_client_connect(int sockfd, app_state_t *app)
{
    int err;

    worker_t *worker;
    client_t  client;

    log_debug("\n%sFD ? -> Server | Connect:%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);

    // Defer connection handling if we're at max clients
    if(app->nworkers == app->max_clients)
    {
        return;
    }

    // Find an available worker -- If none available (unlikely), create a new worker
    worker = app_find_available_worker(app);
    if(worker == NULL)
    {
        worker_t new_worker;

        // Spawn a new worker and use that instead
        err = 0;
        if(spawn_worker(&new_worker, &err) < 0)
        {
            log_error("handle_client_connect::spawn_worker: %s\n", strerror(err));
            return;
        }

        worker = app_add_worker(app, &new_worker);
        app_poll(app, new_worker.fd);

        if(worker->pid == 0)
        {
            close(sockfd);
            worker_entrypoint();
        }
    }

    // Accept the client connection
    err = 0;
    if(tcp_accept(sockfd, &client, &err) < 0)
    {
        if(err != EINTR)
        {
            log_error("handle_client_connect::accept: %s\n", strerror(errno));
        }

        return;
    }

    log_info("[fd:%d] \"%s:%d\" connect\n", client.fd, client.address, client.port);

    // Assign client to the worker
    err = 0;
    if(assign_client_to_worker(worker, &client, &err) < 0)
    {
        if(err == EBUSY)
        {
            log_error("handle_client_connect::assign_client_to_worker: Worker [PID:%d/FD:%d] already has an active client.\n", worker->pid, worker->fd);
            close(client.fd);
        }
        else
        {
            log_error("handle_client_connect::assign_client_to_worker: %s\n", strerror(err));
        }

        return;
    }
}

ssize_t handle_client_data(int connfd)
{
    char   *buf;
    ssize_t nread;

    char *response;

    nread = read_string(connfd, &buf, BUFLEN, NULL);
    if(nread < 0)
    {
        strhcpy(&response, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
        goto write;
    }

    if(nread == 0)
    {
        free(buf);
        return 0;    // Client Disconnected
    }

    // Report the incoming data
    // log_debug("\n%sFD %d -> Server | Request:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);
    // log_debug("%s\n", buf);    // print the data sent to us

    // Do response stuff
    response = NULL;
    strhcpy(&response, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

write:
    // Report the outgoing data
    // log_debug("\n%sServer -> FD %d | Response:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);    // Should only print if the client hasn't disconnected
    // log_debug("%s\n", response);

    // Write the response
    write(connfd, response, strlen(response));

    // Assumes that responses are heap allocated
    free(response);
    free(buf);

    return nread;
}

ssize_t handle_worker_connect(const worker_t *worker, int fd)
{
    int err;
    int dmnfd;

    dmnfd = accept(fd, NULL, NULL);

    err = 0;
    if(send_fd(dmnfd, worker->client.fd, &err) < 0)
    {
        log_error("main::send_fd: %s\n", strerror(err));
        close(dmnfd);
        return -2;
    }

    close(dmnfd);
    return 0;
}

ssize_t handle_worker_disconnect(worker_t *worker, app_state_t *app)
{
    char *socket_path;

    if(worker->client.fd <= 0)
    {
        log_warn("!!! WARNING: WORKER [PID:%d/FD:%d] DISCONNECTING WITH INVALID CLIENT\n", worker->pid, worker->fd);
    }

    // Create path to socket file for unlinking later
    errno       = 0;
    socket_path = make_string("./%d.sock", worker->pid);
    if(socket_path == NULL)
    {
        return -1;
    }

    // Notify the user that the client has disconnected
    log_debug("\n%sFD %d -> Server | Disconnect:%s\n", ANSI_COLOR_YELLOW, worker->client.fd, ANSI_COLOR_RESET);
    log_info("[fd:%d] \"%s:%d\" disconnect\n", worker->client.fd, worker->client.address, worker->client.port);
    log_debug("Worker[PID:%d] has exited.\n", worker->pid);

    // Cleanup the worker
    app_remove_worker(app, worker->pid);

    // Remove .sock file
    errno = 0;
    if(unlink(socket_path) < 0)
    {
        log_error("handle_worker_disconnect::unlink: %s\n", strerror(errno));
    }

    free(socket_path);
    return 0;
}
