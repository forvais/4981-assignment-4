#ifndef CONTEXT_H
#define CONTEXT_H

typedef void (*function_lib)(void);

typedef struct
{
    function_lib func;
} context_t;

void *load_library(void);

void unload_library(void);

#endif /* CONTEXT_H */
