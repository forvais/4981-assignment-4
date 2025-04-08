#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdint.h>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wc++-compat"

#define WRAP(...) ((parser_wrapper_t[]){__VA_ARGS__})
#define COMBINATOR_START(x, s) ((x)->parse(s, (x)->ctx))

#define COMBINATOR(x)                                                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                                              \
        ((x)->parse), ((x)->ctx)                                                                                                                                                                                                                                   \
    }
#define PARSER(x)                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                              \
        x, NULL                                                                                                                                                                                                                                                    \
    }

#define free_com(x)                                                                                                                                                                                                                                                \
    free((x)->ctx);                                                                                                                                                                                                                                                \
    free((x))

typedef ssize_t (*parser_fn)(const char *string, void *ctx);

typedef struct
{
    parser_fn parser;
    void     *parser_ctx;
} parser_wrapper_t;

typedef struct
{
    parser_fn parse;
    void     *ctx;
} combinator_t;

typedef struct
{
    parser_wrapper_t *parsers;
    size_t            nparsers;
} parser_ctx_t;

typedef struct
{
    parser_wrapper_t *parsers;
    size_t            nparsers;
    ssize_t           min;
    ssize_t           max;
} many_parser_ctx_t;

typedef struct
{
    const char *literal;
} literal_parser_ctx_t;

typedef struct
{
    char *method;
    char *uri;
    char *version;
    char *headers;
    char *body;
} http_request_tokens_t;

// Tokenizers
ssize_t tokenize_request_line(http_request_tokens_t *tokens, const char *request);
ssize_t tokenize_headers(http_request_tokens_t *tokens, const char *request);
ssize_t tokenize_body(http_request_tokens_t *tokens, const char *request);
ssize_t tokenize_http_request(http_request_tokens_t *tokens, const char *request);

// Combinators
ssize_t parser_sequence(const char *string, void *ctx);
ssize_t parser_choice(const char *string, void *ctx);
ssize_t parser_many(const char *string, void *ctx);
ssize_t parser_list(const char *string, void *ctx);
ssize_t parser_literal(const char *string, void *ctx);

combinator_t *sequence(size_t nparsers, parser_wrapper_t *parsers);
combinator_t *choice(size_t nparsers, parser_wrapper_t *parsers);
combinator_t *many(parser_wrapper_t *parser, ssize_t min, ssize_t max);
combinator_t *list(parser_wrapper_t *parser, ssize_t min, ssize_t max);
combinator_t *optional(parser_wrapper_t *parser);
combinator_t *literal(const char *string);

// Atoms
ssize_t achar(const char *string, void *ctx);
ssize_t upalpha(const char *string, void *ctx);
ssize_t loalpha(const char *string, void *ctx);
ssize_t alpha(const char *string, void *ctx);
ssize_t digit(const char *string, void *ctx);
ssize_t cr(const char *string, void *ctx);
ssize_t lf(const char *string, void *ctx);
ssize_t sp(const char *string, void *ctx);
ssize_t ht(const char *string, void *ctx);
ssize_t dblqt(const char *string, void *ctx);

ssize_t crlf(const char *string, void *ctx);
ssize_t lws(const char *string, void *ctx);
ssize_t text(const char *string, void *ctx);
ssize_t hex(const char *string, void *ctx);
ssize_t tspecials(const char *string, void *ctx);
ssize_t ctl(const char *string, void *ctx);
ssize_t token(const char *string, void *ctx);
ssize_t qdtext(const char *string, void *ctx);
ssize_t quoted_string(const char *string, void *ctx);
ssize_t word(const char *string, void *ctx);

ssize_t safe(const char *string, void *ctx);
ssize_t unsafe(const char *string, void *ctx);
ssize_t extra(const char *string, void *ctx);
ssize_t reserved(const char *string, void *ctx);
ssize_t national(const char *string, void *ctx);
ssize_t unreserved(const char *string, void *ctx);
ssize_t escape(const char *string, void *ctx);
ssize_t uchar(const char *string, void *ctx);
ssize_t pchar(const char *string, void *ctx);
ssize_t fsegment(const char *string, void *ctx);
ssize_t segment(const char *string, void *ctx);
ssize_t path(const char *string, void *ctx);
ssize_t param(const char *string, void *ctx);
ssize_t params(const char *string, void *ctx);
ssize_t query(const char *string, void *ctx);
ssize_t fragment(const char *string, void *ctx);
ssize_t scheme(const char *string, void *ctx);
ssize_t net_loc(const char *string, void *ctx);
ssize_t rel_path(const char *string, void *ctx);
ssize_t abs_path(const char *string, void *ctx);
ssize_t net_path(const char *string, void *ctx);
ssize_t relative_uri(const char *string, void *ctx);
ssize_t absolute_uri(const char *string, void *ctx);
ssize_t uri(const char *string, void *ctx);

ssize_t month(const char *string, void *ctx);
ssize_t weekday(const char *string, void *ctx);
ssize_t wkday(const char *string, void *ctx);
ssize_t time(const char *string, void *ctx);
ssize_t date3(const char *string, void *ctx);
ssize_t date2(const char *string, void *ctx);
ssize_t date1(const char *string, void *ctx);
ssize_t asctime_date(const char *string, void *ctx);
ssize_t rfc580_date(const char *string, void *ctx);
ssize_t rfc1123_date(const char *string, void *ctx);
ssize_t http_date(const char *string, void *ctx);
ssize_t date(const char *string, void *ctx);

ssize_t extension_pragma(const char *string, void *ctx);
ssize_t pragma_directive(const char *string, void *ctx);
ssize_t pragma(const char *string, void *ctx);

ssize_t userid_password(const char *string, void *ctx);
// ssize_t basic_cookie(const char *string, void *ctx);
// ssize_t basic_credentials(const char *string, void *ctx);
ssize_t auth_scheme(const char *string, void *ctx);
ssize_t auth_param(const char *string, void *ctx);
// ssize_t credentials(const char *string, void *ctx);
// ssize_t mailbox(const char *string, void *ctx);
ssize_t product_version(const char *string, void *ctx);
ssize_t product(const char *string, void *ctx);
ssize_t ctext(const char *string, void *ctx);
ssize_t comment(const char *string, void *ctx);

// ssize_t authorization(const char *string, void *ctx);
// ssize_t from(const char *string, void *ctx);
ssize_t if_modified_since(const char *string, void *ctx);
ssize_t referer(const char *string, void *ctx);
ssize_t user_agent(const char *string, void *ctx);

ssize_t field_content(const char *string, void *ctx);
ssize_t field_value(const char *string, void *ctx);
ssize_t field_name(const char *string, void *ctx);
ssize_t http_header(const char *string, void *ctx);
ssize_t content_coding(const char *string, void *ctx);
ssize_t attribute(const char *string, void *ctx);
ssize_t value(const char *string, void *ctx);
ssize_t parameter(const char *string, void *ctx);
ssize_t type(const char *string, void *ctx);
ssize_t subtype(const char *string, void *ctx);
ssize_t media_type(const char *string, void *ctx);

ssize_t allow(const char *string, void *ctx);
ssize_t content_encoding(const char *string, void *ctx);
ssize_t content_length(const char *string, void *ctx);
ssize_t content_type(const char *string, void *ctx);
ssize_t expires(const char *string, void *ctx);
ssize_t last_modified(const char *string, void *ctx);
ssize_t extension_header(const char *string, void *ctx);

ssize_t general_header(const char *string, void *ctx);
ssize_t request_header(const char *string, void *ctx);
ssize_t entity_header(const char *string, void *ctx);

ssize_t extension_method(const char *string, void *ctx);
ssize_t authority(const char *string, void *ctx);

ssize_t method(const char *string, void *ctx);
ssize_t request_uri(const char *string, void *ctx);
ssize_t http_version(const char *string, void *ctx);

#endif
