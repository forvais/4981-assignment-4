#include "context.h"
#include <dlfcn.h>
#include <stdlib.h>

static void *dlhandle = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void (*s_test)(void) = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void *load_library(void)
{
    handle = dlopen("./library.so", RTLD_LAZY);
    return handle;
}

static void unload_library(void)
{
    if(dlhandle)
    {
        s_test = NULL;

        dlclose(dlhandle);
        dlhandle = NULL;
    }
}

int reload_library(void)
{
    unload_library();
    load_library();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    s_test = dlsym(dlhandle, "test");

#pragma GCC diagnostic pop
#pragma GCC diagnostic pop

    if(!s_test)
    {
        return -1;
    }

    return 0;
}

void test(void)
{
    if(s_test)
    {
        s_test();
    }
}
