#ifndef IO_H
#define IO_H

#include <unistd.h>

ssize_t read_string(int fd, char **buf, size_t size, int *err);
int     send_fd(int sock, int fd, int *err);
int     recv_fd(int sock, int *err);

#endif
