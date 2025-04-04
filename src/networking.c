#include "networking.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void setup_addr(struct sockaddr_storage *sockaddr, socklen_t *socklen, char *address, in_port_t port);

int tcp_socket(struct sockaddr_storage *sockaddr, int *err)
{
    int sockfd;

    // Create TCP Socket
    errno  = 0;
    sockfd = socket(sockaddr->ss_family, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd < 0)
    {
        *err = errno;
        return -1;
    }

    return sockfd;
}

int tcp_server(char *address, in_port_t port)
{
    int ISETOPTION = 1;
    int err;

    int                     sockfd;
    struct sockaddr_storage sockaddr;
    socklen_t               socklen;

    // Setup socket address
    socklen = 0;
    memset(&sockaddr, 0, sizeof(struct sockaddr_storage));
    setup_addr(&sockaddr, &socklen, address, port);

    // Create tcp socket
    err    = 0;
    sockfd = tcp_socket(&sockaddr, &err);
    if(sockfd < 0)
    {
        errno = err;
        perror("tcp_server::socket");
        sockfd = -1;
        goto exit;
    }

    // Allows for rebinding to address after non-graceful termination
    errno = 0;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&ISETOPTION, sizeof(ISETOPTION)) == -1)
    {
        perror("tcp_server::setsockopt");
        sockfd = -2;
        goto exit;
    }

    // Bind the socket
    errno = 0;
    if(bind(sockfd, (struct sockaddr *)&sockaddr, socklen) < 0)
    {
        perror("tcp_server::bind");
        close(sockfd);
        sockfd = -3;
        goto exit;
    }

    // Enable client connections
    errno = 0;
    if(listen(sockfd, SOMAXCONN) < 0)
    {
        perror("tcp_server::listen");
        close(sockfd);
        sockfd = -4;
        goto exit;
    }

exit:
    return sockfd;
}

int tcp_accept(int sockfd, client_t *client, int *err)
{
    int                connfd;
    struct sockaddr_in connaddr;
    socklen_t          connsize;

    connsize = sizeof(struct sockaddr_in);
    memset(&connaddr, 0, connsize);

    errno  = 0;
    connfd = accept(sockfd, (struct sockaddr *)&connaddr, &connsize);
    if(connfd < 0)
    {
        seterr(errno);
        return -1;
    }

    client->fd      = connfd;
    client->address = inet_ntoa(connaddr.sin_addr);
    client->port    = connaddr.sin_port;

    return connfd;
}

int dmn_server(const char *socket_path, int *err)
{
    int sockfd;

    struct sockaddr_un addr;

    // Setup socket address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    // Create domain socket
    errno  = 0;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd < 0)
    {
        seterr(errno);
        return -1;
    }

    // Bind the socket
    errno = 0;
    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        seterr(errno);
        close(sockfd);
        return -2;
    }

    // Enable client connections
    errno = 0;
    if(listen(sockfd, 1) < 0)
    {
        seterr(errno);
        close(sockfd);
        return -3;
    }

    return sockfd;
}

int dmn_client(const char *socket_path, int *err)
{
    int sockfd;

    struct sockaddr_un addr;

    // Setup socket address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    // Create domain socket
    errno  = 0;
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd < 0)
    {
        seterr(errno);
        return -1;
    }

    // Connect to remote socket
    errno = 0;
    if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        seterr(errno);
        close(sockfd);
        return -3;
    }

    return sockfd;
}

/**
 * Sets up an IPv4 or IPv6 address in a socket address struct.
 */
static void setup_addr(struct sockaddr_storage *sockaddr, socklen_t *socklen, char *address, in_port_t port)
{
    if(is_ipv6(address))
    {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)sockaddr;

        inet_pton(AF_INET6, address, &addr->sin6_addr);
        addr->sin6_family = AF_INET6;
        addr->sin6_port   = htons(port);

        *socklen = sizeof(struct sockaddr_in6);
    }
    else
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)sockaddr;

        addr->sin_addr.s_addr = inet_addr(address);
        addr->sin_family      = AF_INET;
        addr->sin_port        = htons(port);

        *socklen = sizeof(struct sockaddr_in);
    }
}

in_port_t convert_port(const char *str, int *err)
{
    in_port_t port;
    char     *endptr;
    long      val;

    *err  = 0;
    port  = 0;
    errno = 0;
    val   = strtol(str, &endptr, 10);    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    // Check if no digits were found
    if(endptr == str)
    {
        *err = -1;
        goto done;
    }

    // Check for out-of-range errors
    if(val < 0 || val > UINT16_MAX)
    {
        *err = -2;
        goto done;
    }

    // Check for trailing invalid characters
    if(*endptr != '\0')
    {
        *err = -3;
        goto done;
    }

    port = (in_port_t)val;

done:
    return port;
}
