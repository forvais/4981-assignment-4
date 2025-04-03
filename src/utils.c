#include "utils.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
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
