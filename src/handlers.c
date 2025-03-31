#include "handlers.h"
#include "io.h"
#include "logger.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 1024

void handle_client_connect(int sockfd, struct pollfd *pollfds, size_t max_clients)
{
    int                connfd;
    struct sockaddr_in connaddr;
    socklen_t          connsize;

    connsize = sizeof(struct sockaddr_in);
    memset(&connaddr, 0, connsize);

    log_debug("\n%sFD ? -> Server | Connect:%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);

    // Accept the client connection
    errno  = 0;
    connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connsize);
    if(connfd < 0)
    {
        if(errno != EINTR)
        {
            log_error("handle_client_connect::accept: %s\n", strerror(errno));
        }

        return;
    }

    log_info("[fd:%d] \"%s:%d\" connect\n", connfd, inet_ntoa(connaddr.sin_addr), connaddr.sin_port);

    // Add client fd to poll list
    for(size_t offset = 0; offset < max_clients; offset++)
    {
        struct pollfd *client_pollfd = pollfds + offset;

        if(client_pollfd->fd != -1)
        {    // Skip indices that are already occupied by an fd
            continue;
        }

        // On the first pollfd with a fd of (-1)...
        client_pollfd->fd     = connfd;
        client_pollfd->events = POLLIN | POLLHUP | POLLERR;
        log_debug("Added client socket [fd:%d] to poll list.\n", connfd);
        break;
    }
}

void handle_client_disconnect(int connfd, struct pollfd *pollfds, size_t max_clients)
{
    log_debug("\n%sFD %d -> Server | Disconnect:%s\n", ANSI_COLOR_YELLOW, connfd, ANSI_COLOR_RESET);
    log_info("[fd:%d] disconnect\n", connfd);
    for(size_t offset = 0; offset < max_clients; offset++)
    {    // Search for the first pollfd with the client fd...
        struct pollfd *client_pollfd = pollfds + offset;

        if(client_pollfd->fd == connfd)
        {
            log_debug("Client %d removed.\n", connfd);
            memset(client_pollfd, -1, sizeof(struct pollfd));
            break;
        }
    }

    close(connfd);
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
