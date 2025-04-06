#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <unistd.h>

ssize_t read_string(int fd, char **buf, size_t size, int *err);
ssize_t read_file(uint8_t **buf, const char *filepath, size_t size, int *err);
int     send_fd(int sock, int fd, int *err);
int     recv_fd(int sock, int *err);

#endif
