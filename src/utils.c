#include "utils.h"
#include <ctype.h>
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
    return strhncpy(dst, src, strlen(src));
}

char *strhncpy(char **dst, const char *src, size_t bytes)
{
    // Calloc buffer
    errno = 0;
    *dst  = (char *)calloc(bytes + 1, sizeof(char));
    if(*dst == NULL)
    {
        return NULL;
    }

    // Copy str to buffer
    memcpy(*dst, src, bytes);

    return *dst;
}

char *str_toupper(char *str, int *err)
{
    size_t len;

    seterr(0);
    if(str == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

    len = strlen(str);
    for(size_t idx = 0; idx < len; idx++)
    {
        str[idx] = (char)toupper(str[idx]);
    }

    return str;
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

/**
 * Separates a string by a delimiter into an array of strings/tokens.
 */
int explode(char ***tokens, const char *string, const char *delimiter)
{
    int retval;

    char *token;

    size_t ntokens;
    char  *string_copy;
    char  *saveptr1;

    if(string == NULL || strlen(string) == 0)
    {
        retval = -1;
        goto exit;
    }

    // Create a copy of string for use with strtok
    errno       = 0;
    string_copy = strdup(string);
    if(string_copy == NULL)
    {
        perror("explode::strdup");
        retval = -2;
        goto exit;
    }

    // Allocate space for the token_list
    // Becuase the token list is null-terminated, we'll need space for the null terminator
    *tokens = (char **)calloc(1, sizeof(char *));
    if(*tokens == NULL)
    {
        retval = -3;
        goto cleanup_copy;
    }

    // Break the string into tokens and append them to a list
    ntokens = 0;
    do
    {
        token = strtok_r(ntokens == 0 ? string_copy : NULL, delimiter, &saveptr1);
        if(token != NULL)
        {
            // Allocate enough space for a token pointer
            char **tptr;
            errno = 0;
            tptr  = (char **)realloc((void *)*tokens, (ntokens + 2) * sizeof(char *));
            if(tptr == NULL)
            {
                retval = -4;
                goto cleanup_copy;
            }
            *tokens = tptr;

            // Copy token address into array
            strhcpy((*tokens) + ntokens, token);

            ntokens++;
        }
    } while(token != NULL);

    // Null Terminate
    (*tokens)[ntokens] = NULL;

    retval = 0;
    goto cleanup_copy;

cleanup_copy:
    free(string_copy);

exit:
    return retval;
}
