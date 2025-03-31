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

bool  is_ipv6(const char *address);
char *strhcpy(char **dst, const char *src);

#endif
