#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdlib.h>

#define unused(x) ((void)(x))
#define arrlen(x) ((sizeof(x)) / (sizeof((x)[0])))
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

bool is_ipv6(const char *address);

char *strhcpy(char **dst, const char *src);
char *strhncpy(char **dst, const char *src, size_t bytes);
char *str_toupper(char *str, int *err);
char *make_string(const char *fmt, ...) __attribute__((format(printf, 1, 0)));
int   explode(char ***tokens, const char *string, const char *delimiter);

#endif
