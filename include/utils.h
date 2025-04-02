#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

#define seterr(x)                                                                                                                                                                                                                                                  \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        if(err)                                                                                                                                                                                                                                                    \
        {                                                                                                                                                                                                                                                          \
            *err = x;                                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                          \
    } while(0)

#ifdef __clang__
    #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif

int setup_signals(void (*signal_handler_fn)(int sig));

bool  is_ipv6(const char *address);
char *strhcpy(char **dst, const char *src);
char *make_string(const char *fmt, ...) __attribute__((format(printf, 1, 0)));

#endif
