#include "utils.h"
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int setup_signals(void (*signal_handler_fn)(int sig))
{
    struct sigaction sa;

    sa.sa_handler = signal_handler_fn;    // Set handler function for SIGINT
    sigemptyset(&sa.sa_mask);             // Don't block any additional signals
    sa.sa_flags = 0;

    // Register signal handler
    return sigaction(SIGINT, &sa, NULL);
}

bool is_ipv6(const char *address)
{
    if(address == NULL || strchr(address, ';') == NULL)
    {
        return false;
    }

    return true;
}

/* Writes a string to a heap-allocated buffer. */
char *strhcpy(char **dst, const char *src)
{
    // Get string length
    const size_t len = strlen(src);

    // Calloc buffer
    errno = 0;
    *dst  = (char *)calloc(len + 1, sizeof(char));
    if(*dst == NULL)
    {
        return NULL;
    }

    // Copy str to buffer
    memcpy(*dst, src, len);

    return *dst;
}

/* Taken from manpage's snprintf example. */
char *make_string(const char *fmt, ...)
{
    int     n    = 0;
    size_t  size = 0;
    char   *p    = NULL;
    va_list ap;

    /* Determine required size. */

    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);    // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(ap);

    if(n < 0)
    {
        return NULL;
    }

    size = (size_t)n + 1; /* One extra byte for '\0' */
    p    = (char *)malloc(size);
    if(p == NULL)
    {
        return NULL;
    }

    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);    // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(ap);

    if(n < 0)
    {
        free(p);
        return NULL;
    }

    return p;
}
