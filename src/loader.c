#include "loader.h"
#include <dlfcn.h>
#include <stdlib.h>

static void *dlhandle = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static int (*s_request_init)(http_request_t *, const char *, int *)                             = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int (*s_request_parse)(http_request_t *, const char *, int *)                            = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int (*s_request_process)(http_request_t *, http_response_t *, int *)                     = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int (*s_response_write)(const http_response_t *, const http_request_t *, char **, int *) = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int (*s_request_destroy)(http_request_t *, int *)                                        = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static int (*s_response_destroy)(http_response_t *, int *)                                      = NULL;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void load_library(const char *filepath)
{
    dlhandle = dlopen(filepath, RTLD_LAZY);
}

static void unload_library(void)
{
    if(dlhandle)
    {
        s_request_init     = NULL;
        s_request_parse    = NULL;
        s_request_process  = NULL;
        s_response_write   = NULL;
        s_request_destroy  = NULL;
        s_response_destroy = NULL;

        dlclose(dlhandle);
        dlhandle = NULL;
    }
}

int reload_library(const char *filepath)
{
    unload_library();
    load_library(filepath);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    s_request_init     = (int (*)(http_request_t *, const char *, int *))dlsym(dlhandle, "request_init");
    s_request_parse    = (int (*)(http_request_t *, const char *, int *))dlsym(dlhandle, "request_parse");
    s_request_process  = (int (*)(http_request_t *, http_response_t *, int *))dlsym(dlhandle, "request_process");
    s_response_write   = (int (*)(const http_response_t *, const http_request_t *, char **, int *))dlsym(dlhandle, "response_write");
    s_request_destroy  = (int (*)(http_request_t *, int *))dlsym(dlhandle, "request_destroy");
    s_response_destroy = (int (*)(http_response_t *, int *))dlsym(dlhandle, "response_destroy");

#pragma GCC diagnostic pop
#pragma GCC diagnostic pop

    if(!(s_request_init && s_request_parse && s_request_process && s_response_write && s_request_destroy && s_response_destroy))
    {
        return -1;
    }

    return 0;
}

int request_init(http_request_t *request, const char *public_dir, int *err)
{
    return s_request_init ? s_request_init(request, public_dir, err) : -1;
}

int request_parse(http_request_t *request, const char *data, int *err)
{
    return s_request_parse ? s_request_parse(request, data, err) : -1;
}

int request_process(http_request_t *request, http_response_t *response, int *err)
{
    return s_request_process ? s_request_process(request, response, err) : -1;
}

int response_write(const http_response_t *response, const http_request_t *request, char **response_buf, int *err)
{
    return s_response_write ? s_response_write(response, request, response_buf, err) : -1;
}

int request_destroy(http_request_t *request, int *err)
{
    return s_request_destroy ? s_request_destroy(request, err) : -1;
}

int response_destroy(http_response_t *response, int *err)
{
    return s_response_destroy ? s_response_destroy(response, err) : -1;
}
