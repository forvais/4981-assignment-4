#include "io.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
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
