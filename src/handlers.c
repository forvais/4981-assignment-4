#include "handlers.h"
#include "http/http-info.h"
#include "io.h"
#include "loader.h"
#include "logger.h"
#include "ndbm/database.h"
#include "networking.h"
#include "state.h"
#include "utils.h"
#include "worker.h"
#include <arpa/inet.h>
#include <dlfcn.h>
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

void handle_client_connect(int sockfd, app_state_t *app, const char *libhttp_filepath)
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

    if(reload_library(libhttp_filepath) < 0)
    {
        log_error("handle_client_connect::reload_library: %s\n", dlerror());
    }

    // Find an available worker -- If none, backlog the client until space is freed
    worker = app_find_available_worker(app, NULL);
    if(worker == NULL)
    {
        return;
    }

    // Accept the client connection
    err = 0;
    if(tcp_accept(sockfd, &client, &err) < 0)
    {
        if(err != EINTR)
        {
            log_error("handle_client_connect::tcp_accept: %s\n", strerror(errno));
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

    // Scale up workers
    app_set_desired_workers(app, app->desired_workers + 1, NULL);
}

ssize_t handle_client_data(int connfd, DBM *db, const char *public_dir)
{
    char   *buf;
    ssize_t nread;

    // uint8_t *response;
    char   *response_buf;
    ssize_t response_size = 0;

    http_request_t  request;
    http_response_t response;

    nread = read_string(connfd, &buf, BUFLEN, NULL);
    if(nread < 0)
    {
        strhcpy(&response_buf, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
        response_size = (ssize_t)strlen(response_buf);
        goto write;
    }

    if(nread == 0)
    {
        free(buf);
        return 0;    // Client Disconnected
    }

    // Report the incoming data
    log_debug("\n%sFD %d -> Server | Request:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);
    log_debug("%s\n", buf);    // print the data sent to us

    // Do response stuff
    request_init(&request, public_dir, NULL);
    if(request_parse(&request, buf, NULL) < 0)
    {
        goto internal_server_error;
    }

    if(request_process(&request, &response, NULL) < 0)
    {
        goto internal_server_error;
    }

    log_info("[FD:%d] %s\n", connfd, request.request_uri);

    if(request.method == HTTP_METHOD_POST && request.body_size > 0 && db_insert(db, request.request_uri, request.body, request.body_size, NULL) < 0)
    {
        log_error("handle_client_data::db_insert: Failed to insert record at route (%s)\n", request.request_uri);
    }

    response.http_version = request.http_version;
    response_size         = response_write(&response, &request, &response_buf, NULL);
    if(response_size < 0)
    {
    internal_server_error:
        strhcpy(&response_buf, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
        response_size = (ssize_t)strlen(response_buf);
    }

write:
    // Report the outgoing data
    log_debug("\n%sServer -> FD %d | Response:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);    // Should only print if the client hasn't disconnected
    log_debug("%s\n", response_buf);

    // Write the response
    write(connfd, response_buf, (size_t)response_size);

    // Assumes that responses are heap allocated
    // free(response);
    request_destroy(&request, NULL);
    response_destroy(&response, NULL);
    free(buf);
    free(response_buf);

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
    if(app_remove_worker(app, worker->pid, NULL) < 0)
    {
        log_error("handle_worker_disconnect::app_remove_worker: Failed to remove worker [PID:%d].\n", worker->pid);
    }

    // Remove .sock file
    errno = 0;
    if(unlink(socket_path) < 0)
    {
        log_error("handle_worker_disconnect::unlink: %s\n", strerror(errno));
    }

    // Down scale workers
    app_set_desired_workers(app, app->desired_workers - 1, NULL);

    free(socket_path);
    return 0;
}
