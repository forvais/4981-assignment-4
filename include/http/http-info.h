// cppcheck-suppress-file unusedStructMember
#ifndef HTTP_INFO_H
#define HTTP_INFO_H

#include <stdint.h>
#include <stdlib.h>

typedef enum
{
    HTTP_METHOD_UNKNOWN,
    HTTP_METHOD_GET,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
} HTTP_METHOD;

typedef enum
{
    HTTP_VERSION_UNKNOWN,
    HTTP_VERSION_10,
    HTTP_VERSION_11
} HTTP_VERSION;

typedef enum
{
    HTTP_STATUS_UNKNOWN = 0,
    HTTP_STATUS_100     = 100,
    HTTP_STATUS_101     = 101,
    HTTP_STATUS_102     = 102,
    HTTP_STATUS_103     = 103,
    HTTP_STATUS_200     = 200,
    HTTP_STATUS_201     = 201,
    HTTP_STATUS_202     = 202,
    HTTP_STATUS_203     = 203,
    HTTP_STATUS_204     = 204,
    HTTP_STATUS_205     = 205,
    HTTP_STATUS_206     = 206,
    HTTP_STATUS_207     = 207,
    HTTP_STATUS_208     = 208,
    HTTP_STATUS_226     = 226,
    HTTP_STATUS_300     = 300,
    HTTP_STATUS_301     = 301,
    HTTP_STATUS_302     = 302,
    HTTP_STATUS_303     = 303,
    HTTP_STATUS_304     = 304,
    HTTP_STATUS_305     = 305,
    HTTP_STATUS_306     = 306,
    HTTP_STATUS_307     = 307,
    HTTP_STATUS_308     = 308,
    HTTP_STATUS_400     = 400,
    HTTP_STATUS_401     = 401,
    HTTP_STATUS_402     = 402,
    HTTP_STATUS_403     = 403,
    HTTP_STATUS_404     = 404,
    HTTP_STATUS_405     = 405,
    HTTP_STATUS_406     = 406,
    HTTP_STATUS_407     = 407,
    HTTP_STATUS_408     = 408,
    HTTP_STATUS_409     = 409,
    HTTP_STATUS_410     = 410,
    HTTP_STATUS_411     = 411,
    HTTP_STATUS_412     = 412,
    HTTP_STATUS_413     = 413,
    HTTP_STATUS_414     = 414,
    HTTP_STATUS_415     = 415,
    HTTP_STATUS_416     = 416,
    HTTP_STATUS_417     = 417,
    HTTP_STATUS_418     = 418,
    HTTP_STATUS_421     = 421,
    HTTP_STATUS_422     = 422,
    HTTP_STATUS_423     = 423,
    HTTP_STATUS_424     = 424,
    HTTP_STATUS_425     = 425,
    HTTP_STATUS_426     = 426,
    HTTP_STATUS_428     = 428,
    HTTP_STATUS_429     = 429,
    HTTP_STATUS_431     = 431,
    HTTP_STATUS_451     = 451,
    HTTP_STATUS_500     = 500,
    HTTP_STATUS_501     = 501,
    HTTP_STATUS_502     = 502,
    HTTP_STATUS_503     = 503,
    HTTP_STATUS_504     = 504,
    HTTP_STATUS_505     = 505,
    HTTP_STATUS_506     = 506,
    HTTP_STATUS_507     = 507,
    HTTP_STATUS_508     = 508,
    HTTP_STATUS_510     = 510,
    HTTP_STATUS_511     = 511
} HTTP_STATUS;

typedef struct
{
    char *key;
    char *value;
} http_header_t;

typedef struct
{
    const char *public_dir;

    // Request Line
    HTTP_METHOD  method;
    char        *request_uri;
    HTTP_VERSION http_version;

    // Headers
    http_header_t *headers;
    size_t         nheaders;

    // body
    uint8_t *body;
    size_t   body_size;
} http_request_t;

typedef struct
{
    // Status Line
    HTTP_VERSION http_version;
    HTTP_STATUS  status;

    // Headers
    http_header_t *headers;
    size_t         nheaders;

    // Body
    char  *body;
    size_t body_size;
} http_response_t;

#endif
