#ifndef LOADER_H
#define LOADER_H

#include "http/http-info.h"

int reload_library(void);

int request_init(http_request_t *, const char *, int *);
int request_parse(http_request_t *, const char *, int *);
int request_process(http_request_t *, http_response_t *, int *);
int response_write(const http_response_t *, const http_request_t *, char **, int *);
int request_destroy(http_request_t *, int *);
int response_destroy(http_response_t *, int *);

#endif
