#include "context.h"
#include <dlfcn.h>
#include <stdlib.h>

static void *handle = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void *load_library(void)
{
    handle = dlopen("./library.so", RTLD_LAZY);
    return handle;
}

void unload_library(void)
{
    if(handle)
    {
        dlclose(handle);
        handle = NULL;
    }
}
