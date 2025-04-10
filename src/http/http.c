#include "http/http.h"
#include "http/tokenizer.h"
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFLEN 1024

typedef struct
{
    HTTP_STATUS key;
    const char *value;
} http_status_string_map_t;

typedef struct
{
    HTTP_METHOD key;
    const char *value;
} http_method_string_map_t;

typedef struct
{
    HTTP_VERSION key;
    const char  *value;
} http_version_string_map_t;

static char *make_string(const char *fmt, ...);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static http_status_string_map_t status_msgs[] = {
    {HTTP_STATUS_100, "Continue"                       },
    {HTTP_STATUS_101, "Switching Protocols"            },
    {HTTP_STATUS_102, "Processing"                     },
    {HTTP_STATUS_103, "Early Hints"                    },
    {HTTP_STATUS_200, "OK"                             },
    {HTTP_STATUS_201, "Created"                        },
    {HTTP_STATUS_202, "Accepted"                       },
    {HTTP_STATUS_203, "Non-Authoritative Information"  },
    {HTTP_STATUS_204, "No Content"                     },
    {HTTP_STATUS_205, "Reset Content"                  },
    {HTTP_STATUS_206, "Partial Content"                },
    {HTTP_STATUS_207, "Multi-Status"                   },
    {HTTP_STATUS_208, "Already Reported"               },
    {HTTP_STATUS_226, "IM Used"                        },
    {HTTP_STATUS_300, "Multiple Choices"               },
    {HTTP_STATUS_301, "Moved Permanently"              },
    {HTTP_STATUS_302, "Found"                          },
    {HTTP_STATUS_303, "See Other"                      },
    {HTTP_STATUS_304, "Not Modified"                   },
    {HTTP_STATUS_305, "Use Proxy"                      },
    {HTTP_STATUS_306, "unused"                         },
    {HTTP_STATUS_307, "Temporary Redirect"             },
    {HTTP_STATUS_308, "Permanent Redirect"             },
    {HTTP_STATUS_400, "Bad Request"                    },
    {HTTP_STATUS_401, "Unauthorized"                   },
    {HTTP_STATUS_402, "Payment Required"               },
    {HTTP_STATUS_403, "Forbidden"                      },
    {HTTP_STATUS_404, "Not Found"                      },
    {HTTP_STATUS_405, "Method Not Allowed"             },
    {HTTP_STATUS_406, "Not Acceptable"                 },
    {HTTP_STATUS_407, "Proxy Authentication Required"  },
    {HTTP_STATUS_408, "Request Timeout"                },
    {HTTP_STATUS_409, "Conflict"                       },
    {HTTP_STATUS_410, "Gone"                           },
    {HTTP_STATUS_411, "Length Required"                },
    {HTTP_STATUS_412, "Precondition Failed"            },
    {HTTP_STATUS_413, "Content Too Large"              },
    {HTTP_STATUS_414, "URI Too Long"                   },
    {HTTP_STATUS_415, "Unsupported Media Type"         },
    {HTTP_STATUS_416, "Range Not Satisfiable"          },
    {HTTP_STATUS_417, "Expectation Failed"             },
    {HTTP_STATUS_418, "I'm a teapot"                   },
    {HTTP_STATUS_421, "Misdirected Request"            },
    {HTTP_STATUS_422, "Unprocessable Content"          },
    {HTTP_STATUS_423, "Locked"                         },
    {HTTP_STATUS_424, "Failed Dependency"              },
    {HTTP_STATUS_425, "Too Early"                      },
    {HTTP_STATUS_426, "Upgrade Required"               },
    {HTTP_STATUS_428, "Precondition Required"          },
    {HTTP_STATUS_429, "Too Many Requests"              },
    {HTTP_STATUS_431, "Request Header Fields Too Large"},
    {HTTP_STATUS_451, "Unavailable For Legal Reasons"  },
    {HTTP_STATUS_500, "Internal Server Error"          },
    {HTTP_STATUS_501, "Not Implemented"                },
    {HTTP_STATUS_502, "Bad Gateway"                    },
    {HTTP_STATUS_503, "Service Unavailable"            },
    {HTTP_STATUS_504, "Gateway Timeout"                },
    {HTTP_STATUS_505, "HTTP Version Not Supported"     },
    {HTTP_STATUS_506, "Variant Also Negotiates"        },
    {HTTP_STATUS_507, "Insufficient Storage"           },
    {HTTP_STATUS_508, "Loop Detected"                  },
    {HTTP_STATUS_510, "Not Extended"                   },
    {HTTP_STATUS_511, "Network Authentication Required"}
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static http_method_string_map_t method_names[] = {
    {HTTP_METHOD_GET,  "GET" },
    {HTTP_METHOD_HEAD, "HEAD"},
    {HTTP_METHOD_POST, "POST"}
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static http_version_string_map_t version_names[] = {
    {HTTP_VERSION_10, "HTTP/1.0"},
    {HTTP_VERSION_11, "HTTP/1.1"},
};

// Request
int request_init(http_request_t *request, const char *public_dir, int *err)
{
    seterr(0);
    if(request == NULL || public_dir == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    memset(request, 0, sizeof(http_request_t));
    request->public_dir = public_dir;

    errno            = 0;
    request->headers = (http_header_t *)calloc(1, sizeof(http_header_t));
    if(request->headers == NULL)
    {
        seterr(errno);
        return -2;
    }

    return 0;
}

int request_destroy(http_request_t *request, int *err)
{
    seterr(0);
    if(request == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    destroy_headers(request->headers, &request->nheaders, err);

    free(request->body);
    free(request->request_uri);
    memset(request, 0, sizeof(http_request_t));

    return 0;
}

int request_parse(http_request_t *request, const char *data, int *err)
{
    http_request_tokens_t tokens;

    char *header_str   = NULL;
    char *header_token = NULL;
    char *save_header_token;
    char *save_header_key_token;

    // Tokenize
    seterr(0);
    if(tokenize_http_request(&tokens, data) < 0)
    {
        return -1;
    }

    // Set request line properties
    request->method       = get_http_method_code(tokens.method, NULL);
    request->request_uri  = strdup(strcmp(tokens.uri, "/") == 0 ? "/index.html" : tokens.uri);
    request->http_version = get_http_version_code(tokens.version, NULL);

    // Set header properties
    errno      = 0;
    header_str = strdup(tokens.headers);
    if(header_str == NULL)
    {
        seterr(errno);
        return -3;
    }

    header_token = strtok_r(header_str, "\r\n", &save_header_token);
    while(header_token != NULL)
    {
        char       *header_key   = NULL;
        const char *header_value = NULL;

        // Split the header by `: ` and capture the key and value
        header_key   = strtok_r(header_token, ": ", &save_header_key_token);
        header_value = header_key + strlen(header_key) + 2;    // Assuming the header only has a ": " after the key

        // Add the header
        if(add_header(&request->headers, &request->nheaders, header_key, header_value, err) < 0)
        {
            free(header_str);
            return -4;
        }

        header_token = strtok_r(NULL, "\r\n", &save_header_token);
    }

    // Copy body
    request->body      = (uint8_t *)strdup(tokens.body);
    request->body_size = strlen(tokens.body);

    free(header_str);
    free(tokens.method);
    free(tokens.uri);
    free(tokens.version);
    free(tokens.body);
    free(tokens.headers);
    return 0;
}

int request_process(http_request_t *request, http_response_t *response, int *err)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-default"
    switch(request->method)
    {
        case HTTP_METHOD_GET:
            return handle_get(request, response, err);
        case HTTP_METHOD_POST:
            return handle_post(request, response, err);
        case HTTP_METHOD_HEAD:
            return handle_head(request, response, err);
        case HTTP_METHOD_UNKNOWN:
            return -1;
    }
#pragma GCC diagnostic pop

    return -1;
}

// int request_get_request_line(http_request_t *request, int *err);

// Request - Handlers
int handle_get(http_request_t *request, http_response_t *response, int *err)
{
    bool uri_valid = false;

    int     fd        = -1;
    char   *body      = NULL;
    ssize_t body_size = -1;

    char *filepath             = NULL;
    char *content_length_value = NULL;
    char *content_type_value   = NULL;

    uri_valid = validate_http_uri(request->request_uri);

    if(!uri_valid)
    {    // User has probably tried to backtrack
        response_init(response, HTTP_STATUS_403, NULL);
        goto exit;
    }

    // Create full filepath
    filepath = make_string("%s%s", request->public_dir, request->request_uri);

    // Open the file
    errno = 0;
    fd    = open(filepath, O_RDONLY | O_CLOEXEC);    // NOLINT(android-cloexec-socket)
    if(fd < 0)
    {
        seterr(errno);
        response_init(response, HTTP_STATUS_404, NULL);
        goto exit;
    }

    // Read the file
    seterr(0);
    body_size = read_fd(fd, (uint8_t **)&body, BUFLEN, err);
    if(body_size < 0)
    {
        response_init(response, HTTP_STATUS_404, NULL);
        goto exit;
    }

    // Write file contents to response->body
    if(response_init(response, HTTP_STATUS_200, err) < 0)
    {
        free(body);
        goto exit;
    }

    response->body      = body;
    response->body_size = (size_t)body_size;

    // Remake Content-Type header
    content_type_value = strdup(get_mime_type(filepath));    // NOLINT(clang-analyzer-unix.Malloc)

    if(destroy_header(response->headers, &response->nheaders, "Content-Type", err) < 0)
    {
        response->status = HTTP_STATUS_500;
        goto exit;
    }

    if(add_header(&response->headers, &response->nheaders, "Content-Type", content_type_value, err) < 0)    // NOLINT(clang-analyzer-unix.Malloc)
    {
        response->status = HTTP_STATUS_500;    // NOLINT(clang-analyzer-unix.Malloc)
        goto exit;
    }

exit:
    // Remake Content-Length header
    content_length_value = make_string("%d", response->body ? body_size : 0);    // NOLINT(clang-analyzer-unix.Malloc)

    if(destroy_header(response->headers, &response->nheaders, "Content-Length", err) < 0)    // NOLINT(clang-analyzer-unix.Malloc)
    {
        response->status = HTTP_STATUS_500;
    }

    if(add_header(&response->headers, &response->nheaders, "Content-Length", content_length_value, err) < 0)
    {
        response->status = HTTP_STATUS_500;
    }

    free(content_length_value);
    free(content_type_value);
    free(filepath);
    close(fd);
    return 0;
}

// http_response_t *handle_post(http_request_t *request, int *err);

int handle_head(http_request_t *request, http_response_t *response, int *err)
{
    handle_get(request, response, err);

    response->body = NULL;

    return 0;
}

int handle_post(http_request_t *request, http_response_t *response, int *err)
{
    handle_get(request, response, err);

    return 0;
}

// Response
int response_init(http_response_t *response, HTTP_STATUS status, int *err)
{
    seterr(0);
    if(response == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    memset(response, 0, sizeof(http_response_t));
    response->status = status;

    errno             = 0;
    response->headers = (http_header_t *)calloc(1, sizeof(http_header_t));
    if(response->headers == NULL)
    {
        seterr(errno);
        return -2;
    }

    return 0;
}

int response_destroy(http_response_t *response, int *err)
{
    if(destroy_headers(response->headers, &response->nheaders, err) < 0)
    {
        return -1;
    }

    free(response->body);
    memset(response, 0, sizeof(http_response_t));

    return 0;
}

ssize_t response_write(const http_response_t *response, const http_request_t *request, char **buf, int *err)
{
    size_t buf_size = 0;

    if(response_write_status_line(response, buf, &buf_size, err) < 0)
    {
        return -1;
    }

    if(response_write_headers(response, buf, &buf_size, err) < 0)
    {
        return -2;
    }

    if(response_write_crlf(response, buf, &buf_size, err) < 0)
    {
        return -3;
    }

    if(request->method != HTTP_METHOD_HEAD && !(response->status >= HTTP_STATUS_400 && response->status < HTTP_STATUS_511))    // Do not write body on HEAD requests or 400-599 status'
    {
        if(response_write_body(response, buf, &buf_size, err) < 0)
        {
            return -4;    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
        }
    }

    return (ssize_t)buf_size;
}

// Response - Write components
int response_write_status_line(const http_response_t *response, char **buf, size_t *buf_size, int *err)
{
    char *tbuf;
    int   len;
    int   written;

    const char *version;
    HTTP_STATUS status;
    const char *status_msg;

    seterr(0);
    if(response == NULL || buf == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    version = get_http_version_name(response->http_version, err);
    if(version == NULL)
    {
        return -2;
    }

    status = response->status;
    if(status == HTTP_STATUS_UNKNOWN)
    {
        return -2;
    }

    status_msg = get_http_status_msg(response->status, err);
    if(status_msg == NULL)
    {
        return -2;
    }

    // Get total string length
    len = snprintf(NULL, 0, "%s %u %s\r\n", version, status, status_msg);    // cppcheck-suppress invalidPrintfArgType_uint
    if(len < 0)
    {
        seterr(EIO);
        return -3;
    }

    tbuf = calloc((size_t)len + 1, sizeof(char));
    if(tbuf == NULL)
    {
        seterr(ENOMEM);
        return -3;
    }
    *buf = tbuf;

    // Write to buf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    written = snprintf(tbuf, (size_t)len + 1, "%s %u %s\r\n", version, status, status_msg);    // cppcheck-suppress invalidPrintfArgType_uint
#pragma GCC diagnostic pop
    if(written != len)
    {
        free(tbuf);
        seterr(EIO);
        return -3;
    }

    *buf_size = (size_t)written;

    return 0;
}

int response_write_headers(const http_response_t *response, char **buf, size_t *buf_size, int *err)
{
    char *tbuf;

    seterr(0);
    if(response == NULL || buf == NULL || *buf == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    for(size_t offset = 0; offset < response->nheaders; offset++)
    {
        const http_header_t *header    = response->headers + offset;
        const size_t         key_len   = strlen(header->key);
        const size_t         value_len = strlen(header->value);
        const size_t         total_len = key_len + 2 + value_len + 2;    // [key] [: ](2) [value] [\\r\n](2); The 2 is for the colon and space and other 2 for \\r\n

        // Expand buf mem space
        errno = 0;
        tbuf  = (char *)realloc(*buf, *buf_size + total_len + 1);
        if(tbuf == NULL)
        {
            seterr(errno);
            return -2;
        }
        *buf = tbuf;

        // Write header in buffer
        memcpy(*buf + *buf_size, header->key, key_len);
        memcpy(*buf + *buf_size + key_len, ": ", 2);
        memcpy(*buf + *buf_size + key_len + 2, header->value, value_len);
        memcpy(*buf + *buf_size + key_len + 2 + value_len, "\r\n", 2);
        (*buf)[*buf_size + total_len] = '\0';
        *buf_size += total_len;
    }

    return 0;
}

int response_write_crlf(const http_response_t *response, char **buf, size_t *buf_size, int *err)
{
    char *tbuf;

    seterr(0);
    if(response == NULL || buf == NULL || *buf == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    // Expand buf mem space
    errno = 0;
    tbuf  = realloc(*buf, *buf_size + 2 + 1);
    if(tbuf == NULL)
    {
        seterr(errno);
        return -2;
    }
    *buf = tbuf;

    // Write \r\n into buf
    memcpy(*buf + *buf_size, "\r\n", 2);
    (*buf)[*buf_size + 2] = '\0';
    *buf_size += 2;

    return 0;
}

int response_write_body(const http_response_t *response, char **buf, size_t *buf_size, int *err)
{
    char *tbuf;

    seterr(0);
    if(response == NULL || buf == NULL || *buf == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    // Expand buf mem space
    errno = 0;
    tbuf  = realloc(*buf, *buf_size + response->body_size + 1);
    if(tbuf == NULL)
    {
        seterr(errno);
        return -2;
    }
    *buf = tbuf;

    // Write body into buf
    memcpy(*buf + *buf_size, response->body, response->body_size);
    (*buf)[*buf_size + response->body_size] = '\0';
    *buf_size += response->body_size;

    return 0;
}

// Headers
int add_header(http_header_t **headers, size_t *nheaders, const char *key, const char *value, int *err)
{
    http_header_t *theaders;

    seterr(0);
    if(headers == NULL || *headers == NULL || nheaders == NULL || key == NULL || value == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    // Expand the headers array
    errno    = 0;
    theaders = realloc(*headers, (*nheaders + 1) * sizeof(http_header_t));
    if(theaders == NULL)
    {
        seterr(errno);
        return -3;
    }
    *headers = theaders;

    // Add header to end of list
    // memcpy(&(*headers)[*nheaders], header, sizeof(http_header_t));
    theaders[*nheaders].key   = strdup(key);
    theaders[*nheaders].value = strdup(value);
    *nheaders += 1;

    return 0;
}

http_header_t *create_header(const char *key, const char *value, int *err)
{
    http_header_t *header;

    size_t key_len;
    size_t value_len;

    seterr(0);
    if(key == NULL || value == NULL)
    {
        seterr(EINVAL);
        return NULL;
    }

    key_len   = strlen(key);
    value_len = strlen(value);

    errno  = 0;
    header = (http_header_t *)calloc(1, sizeof(http_header_t));
    if(header == NULL)
    {
        seterr(errno);
        return NULL;
    }

    errno       = 0;
    header->key = (char *)calloc(key_len + 1, sizeof(char));
    if(header->key == NULL)
    {
        seterr(errno);
        free(header);
        return NULL;
    }
    memcpy(header->key, key, key_len);

    errno         = 0;
    header->value = (char *)calloc(value_len + 1, sizeof(char));
    if(header->value == NULL)
    {
        seterr(errno);
        free(header->key);
        free(header);
        return NULL;
    }
    memcpy(header->value, value, value_len);

    return header;
}

int destroy_header(http_header_t *headers, size_t *nheaders, const char *key, int *err)
{
    bool should_shift = false;

    seterr(0);
    if(headers == NULL || nheaders == NULL || key == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    for(size_t offset = 0; offset < *nheaders; offset++)
    {
        const bool     is_last_element = offset == (*nheaders - 1);
        http_header_t *header          = headers + offset;

        if(header == NULL || header->key == NULL || header->value == NULL)
        {
            continue;
        }

        // Remove the header if found
        if(strcmp(header->key, key) == 0)
        {
            free(header->key);
            free(header->value);
            (*nheaders)--;
            should_shift = true;
        }

        // Shift the remaining elements down after removing the intended header
        if(should_shift && !is_last_element)
        {
            memmove(&headers[offset], &headers[offset + 1], sizeof(http_header_t));
            memset(&headers[offset + 1], 0, sizeof(http_header_t));
        }
    }

    return 0;
}

int destroy_headers(http_header_t *headers, size_t *nheaders, int *err)
{
    seterr(0);
    if(headers == NULL || nheaders == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    // This is a niche case where I've allocated one http_header_t slot but nheader is set to "0" because there are
    // technically no headers in that memory space but I've still got to free the empty allocation.
    if(*nheaders == 0)
    {
        *nheaders += 1;
    }

    for(size_t offset = 0; offset < *nheaders; offset++)
    {
        http_header_t *header = headers + offset;
        free(header->key);
        free(header->value);
    }

    free(headers);
    *nheaders = 0;

    return 0;
}

// Validators
bool validate_http_method(const char *method)
{
    if(method == NULL)
    {
        return false;
    }

    for(size_t idx = 0; idx < arrlen(method_names); idx++)
    {
        if(strcmp(method_names[idx].value, method) == 0)
        {
            return true;
        }
    }

    return false;
}

bool validate_http_uri(const char *uri)
{
    bool result = true;

    char       *mod_uri;
    const char *token_;
    char       *save_token;
    int         net_traversals = 0;

    if(uri == NULL)
    {
        return false;
    }

    // Copy the string so strok can modify it
    mod_uri = strdup(uri);

    // Loop through strtok and count the net of non-backtrack (..) segments
    token_ = strtok_r(mod_uri, "/", &save_token);
    while(token_ != NULL)
    {
        net_traversals += strcmp(token_, "..") != 0 ? 1 : -1;
        result = net_traversals >= 0;
        token_ = strtok_r(NULL, "/", &save_token);
    }

    free(mod_uri);
    return result;
}

bool validate_http_version(const char *version)
{
    if(version == NULL)
    {
        return false;
    }

    for(size_t idx = 0; idx < arrlen(method_names); idx++)
    {
        if(strcmp(version_names[idx].value, version) == 0)
        {
            return true;
        }
    }

    return false;
}

// Utils

HTTP_METHOD get_http_method_code(const char *method, int *err)
{
    seterr(0);
    if(method == NULL)
    {
        seterr(EINVAL);
        return HTTP_METHOD_UNKNOWN;
    }

    for(size_t idx = 0; idx < arrlen(status_msgs); idx++)
    {
        if(strcmp(method_names[idx].value, method) == 0)
        {
            return method_names[idx].key;
        }
    }

    return HTTP_METHOD_UNKNOWN;
}

HTTP_VERSION get_http_version_code(const char *version, int *err)
{
    seterr(0);
    if(version == NULL)
    {
        seterr(EINVAL);
        return HTTP_VERSION_UNKNOWN;
    }

    for(size_t idx = 0; idx < arrlen(status_msgs); idx++)
    {
        if(strcmp(version_names[idx].value, version) == 0)
        {
            return version_names[idx].key;
        }
    }

    return HTTP_VERSION_UNKNOWN;
}

const char *get_http_status_msg(HTTP_STATUS status, int *err)
{
    seterr(0);

    for(size_t idx = 0; idx < arrlen(status_msgs); idx++)
    {
        if(status_msgs[idx].key == status)
        {
            return status_msgs[idx].value;
        }
    }

    return NULL;
}

const char *get_http_version_name(HTTP_VERSION version, int *err)
{
    seterr(0);

    for(size_t idx = 0; idx < arrlen(version_names); idx++)
    {
        if(version_names[idx].key == version)
        {
            return version_names[idx].value;
        }
    }

    return NULL;
}

const char *get_mime_type(const char *filepath)
{
    const char *ext = strrchr(filepath, '.');
    if(ext == NULL)
    {
        return "application/octet-stream";
    }

    ext++;

    if(strcasecmp(ext, "txt") == 0)
    {
        return "text/plain";
    }

    if(strcasecmp(ext, "html") == 0)
    {
        return "text/html";
    }

    if(strcasecmp(ext, "js") == 0)
    {
        return "application/javascript";
    }

    if(strcasecmp(ext, "json") == 0)
    {
        return "application/json";
    }

    if(strcasecmp(ext, "css") == 0)
    {
        return "text/css";
    }

    if(strcasecmp(ext, "png") == 0)
    {
        return "image/png";
    }

    if(strcasecmp(ext, "jpeg") == 0 || strcasecmp(ext, "jpg") == 0)
    {
        return "image/jpeg";
    }

    if(strcasecmp(ext, "gif") == 0)
    {
        return "image/gif";
    }

    if(strcasecmp(ext, "swf") == 0)
    {
        return "application/x-shockwave-flash";
    }

    return "application/octet-stream";
}

// Utils - IO

static char *make_string(const char *fmt, ...)
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

ssize_t read_fd(int fd, uint8_t **buf, size_t size, int *err)
{
    ssize_t nread;
    ssize_t tread;

    errno = 0;
    *buf  = (uint8_t *)calloc(size, sizeof(uint8_t));
    if(*buf == NULL)
    {
        seterr(errno);
        close(fd);
        return -2;
    }

    nread = 0;
    do
    {
        char *tbuf = NULL;

        errno = 0;
        tread = read(fd, &(*buf)[nread], size);
        if(tread < 0)
        {
            seterr(errno);
            close(fd);
            free(*buf);
            return -3;
        }

        nread += tread;

        errno = 0;
        tbuf  = (char *)realloc(*buf, (size_t)nread + size);
        if(tbuf == NULL)
        {
            seterr(errno);
            close(fd);
            return -4;    // NOLINT(cppcoreguidelines-no-magic-numbers)
        }
        *buf = (uint8_t *)tbuf;
    } while(tread == (ssize_t)size);

    close(fd);
    return nread;
}
