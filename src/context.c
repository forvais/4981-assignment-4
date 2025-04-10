#include "context.h"
#include <dlfcn.h>
#include <stdlib.h>

static void *handle = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void *load_library(void)
{
    handle = dlopen("./library.so", RTLD_LAZY);
    return handle;
}

static void unload_library(void)
{
    if(handle)
    {
        dlclose(handle);
        handle = NULL;
    }
}

void* reload_library(void)
{
    unload_library();
    return load_library();
}
