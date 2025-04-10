#ifndef CONTEXT_H
#define CONTEXT_H

typedef void (*function_lib)(void);

typedef struct
{
    function_lib func;
} context_t;

void* reload_library(void);

#endif /* CONTEXT_H */
