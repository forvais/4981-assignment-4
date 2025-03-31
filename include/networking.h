#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>

typedef struct
{
    int       fd;
    char     *address;
    in_port_t port;
} client_t;

int       tcp_socket(struct sockaddr_storage *sockaddr, int *err);
int       tcp_server(char *address, in_port_t port);
int       tcp_accept(int sockfd, client_t *client, int *err);
in_port_t convert_port(const char *str, int *err);

#endif
