#include "io.h"
#include "logger.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

ssize_t read_string(int fd, char **buf, size_t size, int *err)
{
    ssize_t nread;

    // Check if our size is greater than 1 or if fd is invalid
    if(size <= 1 || fd < 0)
    {
        seterr(EINVAL);
        return -1;
    }

    // Set the read to non-blocking
    errno = 0;
    if(fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
    {
        seterr(errno);
        return -2;
    }

    // Allocate [size] heap memory for buf
    errno = 0;
    *buf  = (char *)calloc(size, sizeof(char));
    if(*buf == NULL)
    {
        seterr(errno);
        return -3;
    }

    nread = 0;
    do
    {
        char   *tbuf;
        ssize_t tread;

        // Read [size - 1] bytes from fd
        errno = 0;
        tread = read(fd, *buf + nread, size - 1);
        if(tread < 0)
        {
            if(errno == EAGAIN)
            {
                // It is assumed that the client has sent the entire request all at once.
                // and once we have to "wait for more data", it's likely we've read all the data.
                return nread;
            }

            seterr(errno);
            return -4;
        }

        // if read returned EOF, return the total read size
        if(tread == 0)
        {
            break;
        }

        // Add to total count
        nread += tread;

        // Null terminate last character
        (*buf)[nread] = '\0';

        // Add another [size] bytes into buf
        errno = 0;
        tbuf  = (char *)realloc(*buf, (size_t)nread + size);
        if(tbuf == NULL)
        {
            seterr(errno);
            return -5;    // NOLINT
        }
        *buf = tbuf;
    } while(1);

    return nread;
}

int send_fd(int sock, int fd, int *err)
{
    struct iovec    io;
    struct msghdr   msg = {0};
    struct cmsghdr *cmsg;

    char buf[1] = {0};
    char control[CMSG_SPACE(sizeof(int))];

    io.iov_base = buf;
    io.iov_len  = sizeof(buf);

    msg.msg_iov        = &io;
    msg.msg_iovlen     = 1;
    msg.msg_control    = control;
    msg.msg_controllen = sizeof(control);

    cmsg             = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    errno = 0;
    if(sendmsg(sock, &msg, 0) < 0)
    {
        seterr(errno);
        return -1;
    }

    return 0;
}

int recv_fd(int sock, int *err)
{
    struct iovec    io;
    struct msghdr   msg = {0};
    struct cmsghdr *cmsg;

    char control[CMSG_SPACE(sizeof(int))];
    char buf[1];
    int  fd;

    io.iov_base = buf;
    io.iov_len  = sizeof(buf);

    msg.msg_iov        = &io;
    msg.msg_iovlen     = 1;
    msg.msg_control    = control;
    msg.msg_controllen = sizeof(control);

    errno = 0;
    if(recvmsg(sock, &msg, 0) < 0)
    {
        seterr(errno);
        return -1;
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if(!(cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS))
    {
        return -2;
    }

    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));

    return fd;
}
