#include "handlers.h"
#include "io.h"
#include "logger.h"
#include "networking.h"
#include "state.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 1024

void handle_client_connect(int sockfd, app_state_t *app)
{
    int err;

    client_t client;

    log_debug("\n%sFD ? -> Server | Connect:%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);

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

    // Add client to a "global" list
    app_add_client(app, &client);
}

void handle_client_disconnect(int connfd, app_state_t *app)
{
    const client_t *client;

    // Find the client object by fd
    client = app_find_client(app, connfd);
    if(client == NULL)
    {
        log_error("handle_client_disconnect::app_find_client: Failed to find client with fd:%d\n", connfd);
        return;
    }

    // Notify the user that the client has disconnected
    log_debug("\n%sFD %d -> Server | Disconnect:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);
    log_info("[fd:%d] \"%s:%d\" disconnect\n", connfd, client->address, client->port);

    // Remove the client from the "global" list
    app_remove_client(app, connfd);
    log_debug("Client %d removed.\n", connfd);
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
        return 0;    // Client Disconnected
    }

    // Report the incoming data
    log_debug("\n%sFD %d -> Server | Request:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);
    log_debug("%s\n", buf);    // print the data sent to us

    // Do response stuff
    response = NULL;
    strhcpy(&response, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

write:
    // Report the outgoing data
    log_debug("\n%sServer -> FD %d | Response:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);    // Should only print if the client hasn't disconnected
    log_debug("%s\n", response);

    // Write the response
    write(connfd, response, strlen(response));

    // Assumes that responses are heap allocated
    free(response);

    return nread;
}
