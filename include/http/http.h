// cppcheck-suppress-file unusedStructMember

#ifndef HTTP_H
#define HTTP_H

#include "http/http-info.h"
#include <stdbool.h>
#include <stdint.h>
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

/**
 * response_init: Set error by default
 * response_to_string:
 *  if HTTP_404: write errormsg;
 *  if HTTP_200: write errormsg
 */

// Request
int request_init(http_request_t *request, const char *public_dir, int *err);
int request_destroy(http_request_t *request, int *err);
int request_parse(http_request_t *request, const char *data, int *err);
int request_process(http_request_t *request, http_response_t *response, int *err);
// int              request_get_request_line(http_request_t *request, int *err);

// Request - Handlers
int handle_get(http_request_t *request, http_response_t *response, int *err);
int handle_head(http_request_t *request, http_response_t *response, int *err);
int handle_post(http_request_t *request, http_response_t *response, int *err);

// Response
int     response_init(http_response_t *response, HTTP_STATUS status, int *err);
int     response_destroy(http_response_t *response, int *err);
ssize_t response_write(const http_response_t *response, const http_request_t *request, char **buf, int *err);

// Response - Write components
int response_write_status_line(const http_response_t *response, char **buf, size_t *buf_size, int *err);
int response_write_headers(const http_response_t *response, char **buf, size_t *buf_size, int *err);
int response_write_crlf(const http_response_t *response, char **buf, size_t *buf_size, int *err);
int response_write_body(const http_response_t *response, char **buf, size_t *buf_size, int *err);

// Headers
int            add_header(http_header_t **headers, size_t *nheaders, const char *key, const char *value, int *err);
http_header_t *create_header(const char *key, const char *value, int *err);
int            destroy_header(http_header_t *headers, size_t *nheaders, const char *key, int *err);
int            destroy_headers(http_header_t *headers, size_t *nheaders, int *err);

// Validators
bool validate_http_method(const char *method);
bool validate_http_uri(const char *uri);
bool validate_http_version(const char *version);

// Utils

HTTP_METHOD  get_http_method_code(const char *method, int *err);
HTTP_VERSION get_http_version_code(const char *version, int *err);
const char  *get_http_status_msg(HTTP_STATUS status, int *err);
const char  *get_http_version_name(HTTP_VERSION version, int *err);
const char  *get_mime_type(const char *filepath);

// Utils - IO
// char   *make_string(const char *fmt, ...) __attribute__((format(printf, 1, 0)));
ssize_t read_fd(int fd, uint8_t **buf, size_t size, int *err);

#endif
