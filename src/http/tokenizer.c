#include "http/tokenizer.h"
#include "utils.h"
#include <string.h>

#define ASCII_HT 10
#define ASCII_LF 10
#define ASCII_CR 13
#define ASCII_CTRL_CHARS_END 31
#define ASCII_SP 32
#define ASCII_DBLQT 34
#define ASCII_ZERO 48
#define ASCII_NINE 57
#define ASCII_UP_A 65
#define ASCII_UP_Z 90
#define ASCII_LO_A 97
#define ASCII_LO_Z 122
#define ASCII_DEL 127
#define ASCII_PRINTABLE_CHARACTERS_END 127

ssize_t tokenize_request_line(http_request_tokens_t *tokens, const char *request)
{
    ssize_t base;
    ssize_t offset;

    combinator_t *many_spaces = many(WRAP(PARSER(sp)), 1, -1);

    offset = 0;

    // METHOD
    base = offset;
    offset += method(request + offset, NULL);
    if((offset - base) < 0)
    {
        offset = base;
        goto cleanup;
    }
    strhncpy(&tokens->method, request + base, (size_t)(offset - base));

    // 1*SP
    offset += COMBINATOR_START(many_spaces, request + offset);

    // REQUEST-URI
    base = offset;
    offset += request_uri(request + offset, NULL);
    if((offset - base) < 0)
    {
        offset = base;
        goto cleanup;
    }
    strhncpy(&tokens->uri, request + base, (size_t)(offset - base));

    // 1*SP
    offset += COMBINATOR_START(many_spaces, request + offset);

    // HTTP-VERSION
    base = offset;
    offset += http_version(request + offset, NULL);
    if((offset - base) < 0)
    {
        offset = base;
        goto cleanup;
    }
    strhncpy(&tokens->version, request + base, (size_t)(offset - base));

    // CRLF
    offset += crlf(request + offset, NULL);

cleanup:
    free_com(many_spaces);
    return offset;
}

ssize_t tokenize_headers(http_request_tokens_t *tokens, const char *request)
{
    ssize_t base;
    ssize_t offset;

    combinator_t *com_headers = many(WRAP(PARSER(http_header)), 0, -1);

    base   = 0;
    offset = COMBINATOR_START(com_headers, request);
    if(offset < 0)
    {
        goto cleanup;
    }
    strhncpy(&tokens->headers, request, (size_t)(offset - base));

cleanup:
    free_com(com_headers);
    return offset;
}

// size_t tokenize_body(http_request_tokens_t *tokens, const char *request);

ssize_t tokenize_http_request(http_request_tokens_t *tokens, const char *request)
{
    ssize_t base;
    ssize_t offset;

    memset(tokens, 0, sizeof(http_request_tokens_t));
    offset = 0;

    // Request Line
    base = offset;
    offset += tokenize_request_line(tokens, request + offset);
    if(offset == base)
    {
        return -1;
    }

    // Request Line
    base = offset;
    offset += tokenize_headers(tokens, request + offset);

    // CRLF
    offset += crlf(request + offset, NULL);
    if(offset == base)
    {
        return -1;
    }

    // Body
    strhcpy(&tokens->body, request + offset);

    return offset;
}

ssize_t parser_sequence(const char *string, void *ctx)
{
    ssize_t       nprocessed;
    parser_ctx_t *args = (parser_ctx_t *)ctx;

    nprocessed = 0;

    if(string == NULL || ctx == NULL)
    {
        goto exit;
    }

    for(size_t idx = 0; idx < args->nparsers; idx++)
    {
        const parser_wrapper_t *wrapper = &args->parsers[idx];
        ssize_t                 tprocessed;

        if(string[(size_t)nprocessed] == '\0')
        {
            nprocessed = -1;
            break;
        }

        tprocessed = wrapper->parser(string + nprocessed, wrapper->parser_ctx);
        if(tprocessed < 0)
        {
            nprocessed = tprocessed;
            goto exit;
        }
        nprocessed += tprocessed;
    }

exit:
    return nprocessed;
}

combinator_t *sequence(size_t nparsers, parser_wrapper_t *parsers)
{
    parser_ctx_t *ctx;
    combinator_t *combinator;

    ctx           = (parser_ctx_t *)calloc(1, sizeof(parser_ctx_t));
    ctx->parsers  = parsers;
    ctx->nparsers = nparsers;

    combinator        = (combinator_t *)calloc(1, sizeof(combinator_t));
    combinator->parse = parser_sequence;
    combinator->ctx   = ctx;

    return combinator;
}

ssize_t parser_choice(const char *string, void *ctx)
{
    ssize_t       nprocessed;
    parser_ctx_t *args = (parser_ctx_t *)ctx;

    nprocessed = 0;

    if(string == NULL || ctx == NULL)
    {
        goto exit;
    }

    for(size_t idx = 0; idx < args->nparsers; idx++)
    {
        const parser_wrapper_t *wrapper = &args->parsers[idx];

        // if(string[nprocessed] == '\0')
        // {
        //     break;
        // }
        //
        nprocessed = wrapper->parser(string, wrapper->parser_ctx);
        if(nprocessed > 0)
        {
            goto exit;
        }
    }

exit:
    return nprocessed;
}

combinator_t *choice(size_t nparsers, parser_wrapper_t *parsers)
{
    parser_ctx_t *ctx;
    combinator_t *combinator;

    ctx           = (parser_ctx_t *)calloc(1, sizeof(parser_ctx_t));
    ctx->parsers  = parsers;
    ctx->nparsers = nparsers;

    combinator        = (combinator_t *)calloc(1, sizeof(combinator_t));
    combinator->parse = parser_choice;
    combinator->ctx   = ctx;

    return combinator;
}

ssize_t parser_many(const char *string, void *ctx)
{
    many_parser_ctx_t *args = (many_parser_ctx_t *)ctx;

    parser_wrapper_t *wrapper;
    ssize_t           nprocessed;
    size_t            count;

    nprocessed = 0;

    if(string == NULL || ctx == NULL)
    {
        goto exit;
    }

    wrapper = args->parsers;

    for(count = 0; count < (size_t)args->max || args->max == -1; count++)
    {
        ssize_t tprocessed;

        if(string[nprocessed] == '\0')
        {
            break;
        }

        tprocessed = wrapper->parser(string + nprocessed, wrapper->parser_ctx);
        if(tprocessed < 0)
        {
            int met_min_counts = count >= (size_t)args->min;

            // if(met_min_counts && nprocessed == 0)
            // {
            //     return -1;
            // }

            return met_min_counts ? nprocessed : -1;
        }
        nprocessed += tprocessed;
    }

exit:
    return nprocessed;
}

combinator_t *many(parser_wrapper_t *parser, ssize_t min, ssize_t max)
{
    many_parser_ctx_t *ctx;
    combinator_t      *combinator;

    ctx           = (many_parser_ctx_t *)calloc(1, sizeof(many_parser_ctx_t));
    ctx->parsers  = parser;
    ctx->nparsers = 1;
    ctx->min      = min;
    ctx->max      = max;

    combinator        = (combinator_t *)calloc(1, sizeof(combinator_t));
    combinator->parse = parser_many;
    combinator->ctx   = ctx;

    return combinator;
}

ssize_t parser_list(const char *string, void *ctx)
{
    many_parser_ctx_t *args = (many_parser_ctx_t *)ctx;

    parser_wrapper_t *wrapper;
    ssize_t           nprocessed;
    size_t            count;

    // ( *LWS element *( *LWS "," *LWS element ))

    nprocessed = 0;

    if(string == NULL || ctx == NULL || args->min < 1)
    {
        goto exit;
    }

    wrapper = args->parsers;

    {
        combinator_t *comma                 = literal(",");
        combinator_t *zero_or_more_lws      = many(WRAP(PARSER(lws)), 0, -1);
        combinator_t *leading_element       = sequence(4, WRAP(COMBINATOR(zero_or_more_lws), COMBINATOR(comma), COMBINATOR(zero_or_more_lws), {wrapper->parser, wrapper->parser_ctx}));
        combinator_t *many_leading_elements = many(WRAP(COMBINATOR(leading_element)), 0, -1);

        combinator_t *com_list = sequence(3, WRAP(COMBINATOR(zero_or_more_lws), {wrapper->parser, wrapper->parser_ctx}, COMBINATOR(many_leading_elements)));

        for(count = 0; count < (size_t)args->max || args->max == -1; count++)
        {
            ssize_t tprocessed;

            if(string[nprocessed] == '\0')
            {
                break;
            }

            tprocessed = COMBINATOR_START(com_list, string);
            if(tprocessed < 0)
            {
                nprocessed = count >= (size_t)args->min ? nprocessed : 0;
                break;
            }
            nprocessed += tprocessed;
        }

        free_com(com_list);
        free_com(many_leading_elements);
        free_com(leading_element);
        free_com(zero_or_more_lws);
        free_com(comma);
    }

exit:
    return nprocessed;
}

combinator_t *list(parser_wrapper_t *parser, ssize_t min, ssize_t max)
{
    many_parser_ctx_t *ctx;
    combinator_t      *combinator;

    ctx           = (many_parser_ctx_t *)calloc(1, sizeof(many_parser_ctx_t));
    ctx->parsers  = parser;
    ctx->nparsers = 1;
    ctx->min      = min;
    ctx->max      = max;

    combinator        = (combinator_t *)calloc(1, sizeof(combinator_t));
    combinator->parse = parser_many;
    combinator->ctx   = ctx;

    return combinator;
}

combinator_t *optional(parser_wrapper_t *parser)
{
    return many(parser, 0, 1);
}

ssize_t parser_literal(const char *string, void *ctx)
{
    const literal_parser_ctx_t *args = (literal_parser_ctx_t *)ctx;
    size_t                      len  = strlen(args->literal);

    if(strncmp(string, args->literal, len) == 0)
    {
        return (ssize_t)len;
    }

    return -1;
}

combinator_t *literal(const char *string)
{
    literal_parser_ctx_t *ctx;
    combinator_t         *combinator;

    ctx          = (literal_parser_ctx_t *)calloc(1, sizeof(literal_parser_ctx_t));
    ctx->literal = string;

    combinator        = (combinator_t *)calloc(1, sizeof(combinator_t));
    combinator->parse = parser_literal;
    combinator->ctx   = ctx;

    return combinator;
}

// Atoms
ssize_t achar(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] <= ASCII_PRINTABLE_CHARACTERS_END) ? 1 : -1;
}

ssize_t upalpha(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] >= ASCII_UP_A && (unsigned char)string[0] <= ASCII_UP_Z) ? 1 : -1;
}

ssize_t loalpha(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] >= ASCII_LO_A && (unsigned char)string[0] <= ASCII_LO_Z) ? 1 : -1;
}

ssize_t alpha(const char *string, void *ctx)
{
    combinator_t *loalpha_upalpha_choice = choice(2, WRAP(PARSER(loalpha), PARSER(upalpha)));

    ssize_t result = COMBINATOR_START(loalpha_upalpha_choice, string);

    unused(ctx);

    free_com(loalpha_upalpha_choice);
    return result;
}

ssize_t digit(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] >= ASCII_ZERO && (unsigned char)string[0] <= ASCII_NINE) ? 1 : -1;
}

ssize_t cr(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] == ASCII_CR) ? 1 : -1;
}

ssize_t lf(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] == ASCII_LF) ? 1 : -1;
}

ssize_t sp(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] == ASCII_SP) ? 1 : -1;
}

ssize_t ht(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] == ASCII_HT) ? 1 : -1;
}

ssize_t dblqt(const char *string, void *ctx)
{
    unused(ctx);
    return ((unsigned char)string[0] == ASCII_DBLQT) ? 1 : -1;
}

ssize_t crlf(const char *string, void *ctx)
{
    combinator_t *crlf_seq = sequence(2, WRAP(PARSER(cr), PARSER(lf)));

    ssize_t result = COMBINATOR_START(crlf_seq, string);

    unused(ctx);

    free_com(crlf_seq);
    return result;
}

ssize_t lws(const char *string, void *ctx)
{
    combinator_t *com_opt_crlf          = optional(WRAP(PARSER(crlf)));
    combinator_t *com_sp_ht_choice      = choice(2, WRAP(PARSER(sp), PARSER(ht)));
    combinator_t *com_many_sp_ht_choice = many(WRAP(COMBINATOR(com_sp_ht_choice)), 1, -1);
    combinator_t *com_lws               = sequence(2, WRAP(COMBINATOR(com_opt_crlf), COMBINATOR(com_many_sp_ht_choice)));

    ssize_t result = COMBINATOR_START(com_lws, string);
    unused(ctx);

    free_com(com_lws);
    free_com(com_many_sp_ht_choice);
    free_com(com_sp_ht_choice);
    free_com(com_opt_crlf);

    return result;
}

ssize_t text(const char *string, void *ctx)    // [LWS] <any OCTET excluding CTLs>
{
    ssize_t result = lws(string, NULL);

    unused(ctx);
    if(result > 0)
    {
        return 1;
    }

    if(ctl(string, NULL) > 0)
    {
        return -1;
    }

    return string[0] != '\0' ? 1 : -1;
}

ssize_t hex(const char *string, void *ctx)
{
    switch(string[0])
    {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return 1;
        default:
            return digit(string, ctx);
    }
}

ssize_t tspecials(const char *string, void *ctx)
{
    if(sp(string, ctx) >= 0)
    {
        return 1;
    }

    if(ht(string, ctx) >= 0)
    {
        return 1;
    }

    switch(string[0])
    {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
            return 1;
        default:
            return -1;
    }
}

ssize_t ctl(const char *string, void *ctx)
{
    unused(ctx);
    return ((string[0] <= ASCII_CTRL_CHARS_END) || (string[0] == ASCII_DEL)) ? 1 : -1;
}

static ssize_t char_except_ctl_or_tspecial(const char *string, void *ctx)
{
    unused(ctx);
    return ((achar(string, ctx) >= 0) && (ctl(string, ctx) < 0) && (tspecials(string, ctx) < 0)) ? 1 : -1;
}

ssize_t token(const char *string, void *ctx)
{
    combinator_t *com_token = many(WRAP(PARSER(char_except_ctl_or_tspecial)), 1, -1);

    ssize_t result = COMBINATOR_START(com_token, string);
    unused(ctx);

    free_com(com_token);
    return result;
}

ssize_t qdtext(const char *string, void *ctx)
{
    unused(ctx);

    if(lws(string, ctx) >= 0)
    {
        return 1;
    }

    if(achar(string, ctx) >= 0 && ctl(string, ctx) < 0 && dblqt(string, ctx) < 0)
    {
        return 1;
    }

    return -1;
}

ssize_t quoted_string(const char *string, void *ctx)
{
    combinator_t *com_many_qdtext = many(WRAP(PARSER(qdtext)), 0, -1);

    combinator_t *com_quoted_string = sequence(3, WRAP(PARSER(dblqt), COMBINATOR(com_many_qdtext), PARSER(dblqt)));

    ssize_t result = COMBINATOR_START(com_quoted_string, string);
    unused(ctx);

    free_com(com_quoted_string);
    free_com(com_many_qdtext);
    return result;
}

ssize_t word(const char *string, void *ctx)
{
    combinator_t *com_word = choice(2, WRAP(PARSER(token), PARSER(quoted_string)));

    ssize_t result = COMBINATOR_START(com_word, string);
    unused(ctx);

    free_com(com_word);
    return result;
}

ssize_t safe(const char *string, void *ctx)
{
    unused(ctx);
    switch(string[0])
    {
        case '$':
        case '-':
        case '_':
        case '.':
            return 1;
        default:
            return -1;
    }
}

ssize_t unsafe(const char *string, void *ctx)
{
    switch(string[0])
    {
        case '"':
        case '#':
        case '%':
        case '<':
        case '>':
            return 1;
        default:
            return ((ctl(string, ctx) >= 0) || (sp(string, ctx) >= 0)) ? 1 : -1;
    }
}

ssize_t reserved(const char *string, void *ctx)
{
    unused(ctx);
    switch(string[0])
    {
        case ';':
        case '/':
        case '?':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
            return 1;
        default:
            return -1;
    }
}

ssize_t extra(const char *string, void *ctx)
{
    unused(ctx);
    switch(string[0])
    {
        case '!':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ',':
            return 1;
        default:
            return -1;
    }
}

ssize_t national(const char *string, void *ctx)
{
    return ((alpha(string, ctx) >= 0) || (digit(string, ctx) >= 0) || (reserved(string, ctx) >= 0) || (extra(string, ctx) >= 0) || (safe(string, ctx) >= 0) || (unsafe(string, ctx) >= 0)) ? -1 : 1;
}

ssize_t unreserved(const char *string, void *ctx)
{
    return ((alpha(string, ctx) >= 0) || (digit(string, ctx) == 1) || (safe(string, ctx) == 1) || (extra(string, ctx) == 1) || (national(string, ctx) == 1)) ? 1 : -1;
}

ssize_t escape(const char *string, void *ctx)
{
    combinator_t *percent = literal("%");

    combinator_t *com_escape = sequence(3, WRAP(COMBINATOR(percent), PARSER(hex), PARSER(hex)));

    ssize_t result = COMBINATOR_START(com_escape, string);
    unused(ctx);

    free_com(com_escape);
    free_com(percent);
    return result;
}

ssize_t uchar(const char *string, void *ctx)
{
    combinator_t *com_uchar = choice(2, WRAP(PARSER(unreserved), PARSER(escape)));

    ssize_t result = COMBINATOR_START(com_uchar, string);
    unused(ctx);

    free_com(com_uchar);
    return result;
}

ssize_t pchar(const char *string, void *ctx)
{
    unused(ctx);
    switch(string[0])
    {
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
            return 1;
        default:
            return (uchar(string, ctx) >= 0) ? 1 : -1;
    }
}

ssize_t fsegment(const char *string, void *ctx)
{
    combinator_t *many_pchar = many(WRAP(PARSER(pchar)), 1, -1);

    ssize_t result = COMBINATOR_START(many_pchar, string);
    unused(ctx);

    free_com(many_pchar);
    return result;
}

ssize_t segment(const char *string, void *ctx)
{
    combinator_t *many_pchar = many(WRAP(PARSER(pchar)), 0, -1);

    ssize_t result = COMBINATOR_START(many_pchar, string);
    unused(ctx);

    free_com(many_pchar);
    return result;
}

ssize_t path(const char *string, void *ctx)
{
    combinator_t *forward_slash = literal("/");

    combinator_t *slash_segment       = sequence(2, WRAP(COMBINATOR(forward_slash), PARSER(segment)));
    combinator_t *many_slash_segments = many(WRAP(COMBINATOR(slash_segment)), 0, -1);

    combinator_t *com_path = sequence(2, WRAP(PARSER(fsegment), COMBINATOR(many_slash_segments)));

    ssize_t result = COMBINATOR_START(com_path, string);
    unused(ctx);

    free_com(com_path);
    free_com(many_slash_segments);
    free_com(slash_segment);
    free_com(forward_slash);
    return result;
}

ssize_t param(const char *string, void *ctx)
{
    combinator_t *forward_slash = literal("/");

    combinator_t *com_pchar_slash_choice = choice(2, WRAP(PARSER(pchar), COMBINATOR(forward_slash)));

    combinator_t *com_many_pchar_slash_choice = many(WRAP(COMBINATOR(com_pchar_slash_choice)), 0, -1);

    ssize_t result = COMBINATOR_START(com_many_pchar_slash_choice, string);
    unused(ctx);

    free_com(com_many_pchar_slash_choice);
    free_com(com_pchar_slash_choice);
    free_com(forward_slash);
    return result;
}

ssize_t params(const char *string, void *ctx)
{
    combinator_t *semi = literal(";");

    combinator_t *com_semi_param       = sequence(2, WRAP(COMBINATOR(semi), PARSER(param)));
    combinator_t *com_many_semi_params = many(WRAP(COMBINATOR(com_semi_param)), 0, -1);

    combinator_t *com_params = sequence(2, WRAP(PARSER(param), COMBINATOR(com_many_semi_params)));

    ssize_t result = COMBINATOR_START(com_params, string);
    unused(ctx);

    free_com(com_params);
    free_com(com_many_semi_params);
    free_com(com_semi_param);
    free_com(semi);
    return result;
}

ssize_t query(const char *string, void *ctx)
{
    combinator_t *com_uchar_or_reserved = choice(2, WRAP(PARSER(uchar), PARSER(reserved)));

    combinator_t *com_query = many(WRAP(COMBINATOR(com_uchar_or_reserved)), 0, -1);

    ssize_t result = COMBINATOR_START(com_query, string);
    unused(ctx);

    free_com(com_query);
    free_com(com_uchar_or_reserved);
    return result;
}

ssize_t fragment(const char *string, void *ctx)
{
    return query(string, ctx);
}

ssize_t scheme(const char *string, void *ctx)
{
    combinator_t *plus  = literal("+");
    combinator_t *minus = literal("-");
    combinator_t *dot   = literal(".");

    combinator_t *com_choice = choice(5, WRAP(PARSER(alpha), PARSER(digit), COMBINATOR(plus), COMBINATOR(minus), COMBINATOR(dot)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    combinator_t *com_scheme = many(WRAP(COMBINATOR(com_choice)), 1, -1);

    ssize_t result = COMBINATOR_START(com_scheme, string);
    unused(ctx);

    free_com(com_scheme);
    free_com(com_choice);
    free_com(dot);
    free_com(minus);
    free_com(plus);
    return result;
}

ssize_t net_loc(const char *string, void *ctx)
{
    combinator_t *question_mark = literal("?");
    combinator_t *semi          = literal(";");

    combinator_t *com_choice = choice(3, WRAP(PARSER(pchar), COMBINATOR(semi), COMBINATOR(question_mark)));

    combinator_t *com_net_loc = many(WRAP(COMBINATOR(com_choice)), 0, -1);

    ssize_t result = COMBINATOR_START(com_net_loc, string);
    unused(ctx);

    free_com(com_net_loc);
    free_com(com_choice);
    free_com(semi);
    free_com(question_mark);
    return result;
}

ssize_t rel_path(const char *string, void *ctx)
{
    combinator_t *question_mark = literal("?");
    combinator_t *semi          = literal(";");

    combinator_t *com_path                    = optional(WRAP(PARSER(path)));
    combinator_t *com_semi_params             = sequence(2, WRAP(COMBINATOR(semi), PARSER(params)));
    combinator_t *com_optional_semi_params    = optional(WRAP(COMBINATOR(com_semi_params)));
    combinator_t *com_question_query          = sequence(2, WRAP(COMBINATOR(question_mark), PARSER(query)));
    combinator_t *com_optional_question_query = optional(WRAP(COMBINATOR(com_question_query)));

    combinator_t *com_rel_path = sequence(3, WRAP(COMBINATOR(com_path), COMBINATOR(com_optional_semi_params), COMBINATOR(com_optional_question_query)));

    ssize_t result = COMBINATOR_START(com_rel_path, string);
    unused(ctx);

    free_com(com_rel_path);
    free_com(com_optional_question_query);
    free_com(com_question_query);
    free_com(com_optional_semi_params);
    free_com(com_semi_params);
    free_com(com_path);
    free_com(semi);
    free_com(question_mark);
    return result;
}

ssize_t abs_path(const char *string, void *ctx)
{
    combinator_t *semi = literal("/");

    combinator_t *com_abs_path = sequence(2, WRAP(COMBINATOR(semi), PARSER(rel_path)));

    ssize_t result = COMBINATOR_START(com_abs_path, string);
    unused(ctx);

    free_com(com_abs_path);
    free_com(semi);
    return result;
}

ssize_t net_path(const char *string, void *ctx)
{
    combinator_t *dbl_forward = literal("//");

    combinator_t *com_optional_abs_path = optional(WRAP(PARSER(abs_path)));

    combinator_t *com_net_path = sequence(3, WRAP(COMBINATOR(dbl_forward), PARSER(net_loc), COMBINATOR(com_optional_abs_path)));

    ssize_t result = COMBINATOR_START(com_net_path, string);
    unused(ctx);

    free_com(com_net_path);
    free_com(com_optional_abs_path);
    free_com(dbl_forward);
    return result;
}

ssize_t relative_uri(const char *string, void *ctx)
{
    combinator_t *com_relative_uri = choice(3, WRAP(PARSER(net_path), PARSER(abs_path), PARSER(rel_path)));

    ssize_t result = COMBINATOR_START(com_relative_uri, string);
    unused(ctx);

    free_com(com_relative_uri);
    return result;
}

ssize_t absolute_uri(const char *string, void *ctx)
{
    combinator_t *colon = literal(":");

    combinator_t *com_uchar_reserved_choice       = choice(2, WRAP(PARSER(uchar), PARSER(reserved)));
    combinator_t *com_many_uchar_reserved_choices = many(WRAP(COMBINATOR(com_uchar_reserved_choice)), 0, -1);

    combinator_t *com_absolute_uri = sequence(3, WRAP(PARSER(scheme), COMBINATOR(colon), COMBINATOR(com_many_uchar_reserved_choices)));

    ssize_t result = COMBINATOR_START(com_absolute_uri, string);
    unused(ctx);

    free_com(com_absolute_uri);
    free_com(com_many_uchar_reserved_choices);
    free_com(com_uchar_reserved_choice);
    free_com(colon);
    return result;
}

ssize_t uri(const char *string, void *ctx)
{
    combinator_t *hash = literal("#");

    combinator_t *com_abs_rel_choice         = choice(2, WRAP(PARSER(absolute_uri), PARSER(relative_uri)));
    combinator_t *com_hash_fragment          = sequence(2, WRAP(COMBINATOR(hash), PARSER(fragment)));
    combinator_t *com_optional_hash_fragment = optional(WRAP(COMBINATOR(com_hash_fragment)));

    combinator_t *com_uri = sequence(2, WRAP(COMBINATOR(com_abs_rel_choice), COMBINATOR(com_optional_hash_fragment)));

    ssize_t result = COMBINATOR_START(com_uri, string);
    unused(ctx);

    free_com(com_uri);
    free_com(com_optional_hash_fragment);
    free_com(com_hash_fragment);
    free_com(com_abs_rel_choice);
    free_com(hash);
    return result;
}

ssize_t month(const char *string, void *ctx)
{
    combinator_t *jan = literal("Jan");
    combinator_t *feb = literal("Feb");
    combinator_t *mar = literal("Mar");
    combinator_t *apr = literal("Apr");
    combinator_t *may = literal("May");
    combinator_t *jun = literal("Jun");
    combinator_t *jul = literal("Jul");
    combinator_t *aug = literal("Aug");
    combinator_t *sep = literal("Sep");
    combinator_t *oct = literal("Oct");
    combinator_t *nov = literal("Nov");
    combinator_t *dec = literal("Dec");

    combinator_t *com_month = choice(12,    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                                     WRAP(COMBINATOR(jan), COMBINATOR(feb), COMBINATOR(mar), COMBINATOR(apr), COMBINATOR(may), COMBINATOR(jun), COMBINATOR(jul), COMBINATOR(aug), COMBINATOR(sep), COMBINATOR(oct), COMBINATOR(nov), COMBINATOR(dec)));

    ssize_t result = COMBINATOR_START(com_month, string);
    unused(ctx);

    free_com(com_month);
    free_com(jan);
    free_com(feb);
    free_com(mar);
    free_com(apr);
    free_com(may);
    free_com(jun);
    free_com(jul);
    free_com(aug);
    free_com(sep);
    free_com(oct);
    free_com(nov);
    free_com(dec);
    return result;
}

ssize_t weekday(const char *string, void *ctx)
{
    combinator_t *mon = literal("Monday");
    combinator_t *tue = literal("Tuesday");
    combinator_t *wed = literal("Wednesday");
    combinator_t *thu = literal("Thursday");
    combinator_t *fri = literal("Friday");
    combinator_t *sat = literal("Saturday");
    combinator_t *sun = literal("Sunday");

    combinator_t *com_weekday = choice(7, WRAP(COMBINATOR(mon), COMBINATOR(tue), COMBINATOR(wed), COMBINATOR(thu), COMBINATOR(fri), COMBINATOR(sat), COMBINATOR(sun)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_weekday, string);
    unused(ctx);

    free_com(com_weekday);
    free_com(mon);
    free_com(tue);
    free_com(wed);
    free_com(thu);
    free_com(fri);
    free_com(sat);
    free_com(sun);
    return result;
}

ssize_t wkday(const char *string, void *ctx)
{
    combinator_t *mon = literal("Mon");
    combinator_t *tue = literal("Tue");
    combinator_t *wed = literal("Wed");
    combinator_t *thu = literal("Thu");
    combinator_t *fri = literal("Fri");
    combinator_t *sat = literal("Sat");
    combinator_t *sun = literal("Sun");

    combinator_t *com_wkday = choice(7, WRAP(COMBINATOR(mon), COMBINATOR(tue), COMBINATOR(wed), COMBINATOR(thu), COMBINATOR(fri), COMBINATOR(sat), COMBINATOR(sun)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_wkday, string);
    unused(ctx);

    free_com(com_wkday);
    free_com(mon);
    free_com(tue);
    free_com(wed);
    free_com(thu);
    free_com(fri);
    free_com(sat);
    free_com(sun);
    return result;
}

ssize_t time(const char *string, void *ctx)
{
    combinator_t *colon     = literal(":");
    combinator_t *dbl_digit = many(WRAP(PARSER(digit)), 2, 2);

    combinator_t *com_time = sequence(5, WRAP(COMBINATOR(dbl_digit), COMBINATOR(colon), COMBINATOR(dbl_digit), COMBINATOR(colon), COMBINATOR(dbl_digit)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_time, string);
    unused(ctx);

    free_com(com_time);
    free_com(dbl_digit);
    free_com(colon);
    return result;
}

ssize_t date3(const char *string, void *ctx)
{
    combinator_t *digit1 = many(WRAP(PARSER(digit)), 1, 1);
    combinator_t *digit2 = many(WRAP(PARSER(digit)), 2, 2);

    combinator_t *com_sp_digit1 = sequence(2, WRAP(PARSER(sp), COMBINATOR(digit1)));
    combinator_t *com_day       = choice(2, WRAP(COMBINATOR(digit2), COMBINATOR(com_sp_digit1)));

    combinator_t *com_date3 = sequence(3, WRAP(PARSER(month), PARSER(sp), COMBINATOR(com_day)));

    ssize_t result = COMBINATOR_START(com_date3, string);
    unused(ctx);

    free_com(com_date3);
    free_com(com_day);
    free_com(com_sp_digit1);
    free_com(digit2);
    free_com(digit1);
    return result;
}

ssize_t date2(const char *string, void *ctx)
{
    combinator_t *digit2 = many(WRAP(PARSER(digit)), 2, 2);
    combinator_t *hyphen = literal("-");

    combinator_t *com_date2 = sequence(5, WRAP(COMBINATOR(digit2), COMBINATOR(hyphen), COMBINATOR(digit2), COMBINATOR(hyphen), COMBINATOR(digit2)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_date2, string);
    unused(ctx);

    free_com(com_date2);
    free_com(hyphen);
    free_com(digit2);
    return result;
}

ssize_t date1(const char *string, void *ctx)
{
    combinator_t *digit2 = many(WRAP(PARSER(digit)), 2, 2);
    combinator_t *digit4 = many(WRAP(PARSER(digit)), 4, 4);

    combinator_t *com_date1 = sequence(5, WRAP(COMBINATOR(digit2), PARSER(sp), PARSER(month), PARSER(sp), COMBINATOR(digit4)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_date1, string);
    unused(ctx);

    free_com(com_date1);
    free_com(digit4);
    free_com(digit2);
    return result;
}

ssize_t asctime_date(const char *string, void *ctx)
{
    combinator_t *digit4 = many(WRAP(PARSER(digit)), 4, 4);

    combinator_t *com_asctime_date = sequence(7, WRAP(PARSER(wkday), PARSER(sp), PARSER(date3), PARSER(sp), PARSER(time), PARSER(sp), COMBINATOR(digit4)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_asctime_date, string);
    unused(ctx);

    free_com(com_asctime_date);
    free_com(digit4);
    return result;
}

ssize_t rfc580_date(const char *string, void *ctx)
{
    combinator_t *comma = literal(",");
    combinator_t *gmt   = literal("GMT");

    combinator_t *com_rfc580_date = sequence(8, WRAP(PARSER(weekday), COMBINATOR(comma), PARSER(sp), PARSER(date2), PARSER(sp), PARSER(time), PARSER(sp), COMBINATOR(gmt)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_rfc580_date, string);
    unused(ctx);

    free_com(com_rfc580_date);
    free_com(gmt);
    free_com(comma);
    return result;
}

ssize_t rfc1123_date(const char *string, void *ctx)
{
    combinator_t *comma = literal(",");
    combinator_t *gmt   = literal("GMT");

    combinator_t *com_rfc580_date = sequence(8, WRAP(PARSER(wkday), COMBINATOR(comma), PARSER(sp), PARSER(date1), PARSER(sp), PARSER(time), PARSER(sp), COMBINATOR(gmt)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_rfc580_date, string);
    unused(ctx);

    free_com(com_rfc580_date);
    free_com(gmt);
    free_com(comma);
    return result;
}

ssize_t http_date(const char *string, void *ctx)
{
    combinator_t *com_http_date = choice(3, WRAP(PARSER(rfc1123_date), PARSER(rfc580_date), PARSER(asctime_date)));

    ssize_t result = COMBINATOR_START(com_http_date, string);
    unused(ctx);

    free_com(com_http_date);
    return result;
}

ssize_t date(const char *string, void *ctx)
{
    combinator_t *date  = literal("Date");
    combinator_t *colon = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);
    combinator_t *com_date     = sequence(4, WRAP(COMBINATOR(date), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(http_date)));

    ssize_t result = COMBINATOR_START(com_date, string);
    unused(ctx);

    free_com(com_date);
    free_com(com_many_lws);
    free_com(colon);
    free_com(date);
    return result;
}

ssize_t extension_pragma(const char *string, void *ctx)
{
    combinator_t *equal = literal("=");

    combinator_t *com_equal_word          = sequence(2, WRAP(COMBINATOR(equal)));
    combinator_t *com_optional_equal_word = optional(WRAP(COMBINATOR(com_equal_word)));

    combinator_t *com_extension_pragma = sequence(2, WRAP(PARSER(token), COMBINATOR(com_optional_equal_word)));

    ssize_t result = COMBINATOR_START(com_extension_pragma, string);
    unused(ctx);

    free_com(com_extension_pragma);
    free_com(com_optional_equal_word);
    free_com(com_equal_word);
    free_com(equal);
    return result;
}

ssize_t pragma_directive(const char *string, void *ctx)
{
    combinator_t *no_cache = literal("no-cache");

    combinator_t *com_pragma_directive = choice(2, WRAP(COMBINATOR(no_cache), PARSER(extension_pragma)));

    ssize_t result = COMBINATOR_START(com_pragma_directive, string);
    unused(ctx);

    free_com(com_pragma_directive);
    free_com(no_cache);
    return result;
}

ssize_t pragma(const char *string, void *ctx)
{
    combinator_t *literal_pragma = literal("pragma");
    combinator_t *colon          = literal(":");

    combinator_t *com_pragma_directive_list = list(WRAP(PARSER(pragma_directive)), 1, -1);
    combinator_t *com_many_lws              = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_pragma = sequence(4, WRAP(COMBINATOR(literal_pragma), COMBINATOR(colon), COMBINATOR(com_many_lws), COMBINATOR(com_pragma_directive_list)));

    ssize_t result = COMBINATOR_START(com_pragma, string);
    unused(ctx);

    free_com(com_pragma);
    free_com(com_many_lws);
    free_com(com_pragma_directive_list);
    free_com(colon);
    free_com(literal_pragma);
    return result;
}

ssize_t userid_password(const char *string, void *ctx)
{
    combinator_t *colon = literal(":");

    combinator_t *com_optional_token = optional(WRAP(PARSER(token)));
    combinator_t *com_many_text      = many(WRAP(PARSER(text)), 0, -1);

    combinator_t *com_userid_password = sequence(3, WRAP(COMBINATOR(com_optional_token), COMBINATOR(colon), COMBINATOR(com_many_text)));

    ssize_t result = COMBINATOR_START(com_userid_password, string);
    unused(ctx);

    free_com(com_userid_password);
    free_com(com_many_text);
    free_com(com_optional_token);
    free_com(colon);
    return result;
}

ssize_t auth_scheme(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t auth_param(const char *string, void *ctx)
{
    combinator_t *equal = literal("=");

    combinator_t *com_auth_param = sequence(3, WRAP(PARSER(token), COMBINATOR(equal), PARSER(quoted_string)));

    ssize_t result = COMBINATOR_START(com_auth_param, string);
    unused(ctx);

    free_com(com_auth_param);
    free_com(equal);
    return result;
}

ssize_t product(const char *string, void *ctx)
{
    combinator_t *forward_slash = literal("/");

    combinator_t *com_forward_product_version          = sequence(2, WRAP(COMBINATOR(forward_slash), PARSER(product_version)));
    combinator_t *com_optional_forward_product_version = optional(WRAP(COMBINATOR(com_forward_product_version)));
    combinator_t *com_product                          = sequence(2, WRAP(PARSER(token), COMBINATOR(com_optional_forward_product_version)));

    ssize_t result = COMBINATOR_START(com_product, string);
    unused(ctx);

    free_com(com_product);
    free_com(com_optional_forward_product_version);
    free_com(com_forward_product_version);
    free_com(forward_slash);
    return result;
}

ssize_t product_version(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t ctext(const char *string, void *ctx)
{
    unused(ctx);
    switch(string[0])
    {
        case '(':
        case ')':
            return 0;
        default:
            return text(string, ctx) ? 1 : -1;
    }
}

ssize_t comment(const char *string, void *ctx)
{
    // "(" *( ctext | comment ) ")"
    combinator_t *lparen = literal("(");
    combinator_t *rparen = literal(")");

    combinator_t *com_ctext_comment      = choice(2, WRAP(PARSER(ctext), PARSER(comment)));
    combinator_t *com_many_ctext_comment = many(WRAP(COMBINATOR(com_ctext_comment)), 0, -1);

    combinator_t *com_comment = sequence(3, WRAP(COMBINATOR(lparen), COMBINATOR(com_many_ctext_comment), COMBINATOR(rparen)));

    ssize_t result = COMBINATOR_START(com_comment, string);
    unused(ctx);

    free_com(com_comment);
    free_com(com_many_ctext_comment);
    free_com(com_ctext_comment);
    free_com(rparen);
    free_com(lparen);
    return result;
}

ssize_t if_modified_since(const char *string, void *ctx)
{
    combinator_t *literal_if_modified_since = literal("If-Modified-Since");
    combinator_t *colon                     = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_if_modified_since = sequence(4, WRAP(COMBINATOR(literal_if_modified_since), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(http_date)));

    ssize_t result = COMBINATOR_START(com_if_modified_since, string);
    unused(ctx);

    free_com(com_if_modified_since);
    free_com(com_many_lws);
    free_com(colon);
    free_com(literal_if_modified_since);
    return result;
}

ssize_t referer(const char *string, void *ctx)
{
    combinator_t *literal_referer = literal("Referer");
    combinator_t *colon           = literal(":");

    combinator_t *com_absolute_uri_relative_uri_choice = choice(2, WRAP(PARSER(absolute_uri), PARSER(relative_uri)));
    combinator_t *com_many_lws                         = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_referer = sequence(4, WRAP(COMBINATOR(literal_referer), COMBINATOR(colon), COMBINATOR(com_many_lws), COMBINATOR(com_absolute_uri_relative_uri_choice)));

    ssize_t result = COMBINATOR_START(com_referer, string);
    unused(ctx);

    free_com(com_referer);
    free_com(com_many_lws);
    free_com(com_absolute_uri_relative_uri_choice);
    free_com(colon);
    free_com(literal_referer);
    return result;
}

ssize_t user_agent(const char *string, void *ctx)
{
    combinator_t *literal_user_agent = literal("Referer");
    combinator_t *colon              = literal(":");

    combinator_t *com_product_comment_choice       = choice(2, WRAP(PARSER(product), PARSER(comment)));
    combinator_t *com_many_product_comment_choices = many(WRAP(COMBINATOR(com_product_comment_choice)), 1, -1);
    combinator_t *com_many_lws                     = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_user_agent = sequence(4, WRAP(COMBINATOR(literal_user_agent), COMBINATOR(colon), COMBINATOR(com_many_lws), COMBINATOR(com_product_comment_choice)));

    ssize_t result = COMBINATOR_START(com_user_agent, string);
    unused(ctx);

    free_com(com_user_agent);
    free_com(com_many_lws);
    free_com(com_many_product_comment_choices);
    free_com(com_product_comment_choice);
    free_com(colon);
    free_com(literal_user_agent);
    return result;
}

ssize_t field_content(const char *string, void *ctx)
{
    combinator_t *com_choice = choice(4, WRAP(PARSER(text), PARSER(token), PARSER(tspecials), PARSER(quoted_string)));

    combinator_t *com_field_content = many(WRAP(COMBINATOR(com_choice)), 0, -1);

    ssize_t result = COMBINATOR_START(com_field_content, string);
    unused(ctx);

    free_com(com_field_content);
    free_com(com_choice);
    return result;
}

ssize_t field_value(const char *string, void *ctx)
{
    combinator_t *com_choice = choice(2, WRAP(PARSER(field_content), PARSER(lws)));

    combinator_t *com_field_value = many(WRAP(COMBINATOR(com_choice)), 0, -1);

    ssize_t result = COMBINATOR_START(com_field_value, string);
    unused(ctx);

    free_com(com_field_value);
    free_com(com_choice);
    return result;
}

ssize_t field_name(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t http_header(const char *string, void *ctx)
{
    combinator_t *colon = literal(":");

    combinator_t *com_optional_value = optional(WRAP(PARSER(field_value)));
    combinator_t *com_http_header    = sequence(5, WRAP(PARSER(field_name), COMBINATOR(colon), PARSER(sp), COMBINATOR(com_optional_value), PARSER(crlf)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_http_header, string);
    unused(ctx);

    free_com(com_http_header);
    free_com(com_optional_value);
    free_com(colon);
    return result;
}

ssize_t content_coding(const char *string, void *ctx)
{
    combinator_t *x_gzip     = literal("x-gzip");
    combinator_t *x_compress = literal("x-compress");

    combinator_t *com_content_coding = choice(3, WRAP(COMBINATOR(x_gzip), COMBINATOR(x_compress), PARSER(token)));

    ssize_t result = COMBINATOR_START(com_content_coding, string);
    unused(ctx);

    free_com(com_content_coding);
    free_com(x_compress);
    free_com(x_gzip);
    return result;
}

ssize_t attribute(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t value(const char *string, void *ctx)
{
    combinator_t *com_value = choice(2, WRAP(PARSER(token), PARSER(quoted_string)));

    ssize_t result = COMBINATOR_START(com_value, string);
    unused(ctx);

    free_com(com_value);
    return result;
}

ssize_t parameter(const char *string, void *ctx)
{
    combinator_t *equal = literal("=");

    combinator_t *com_parameter = sequence(3, WRAP(PARSER(attribute), COMBINATOR(equal), PARSER(value)));

    ssize_t result = COMBINATOR_START(com_parameter, string);
    unused(ctx);

    free_com(com_parameter);
    free_com(equal);
    return result;
}

ssize_t type(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t subtype(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t media_type(const char *string, void *ctx)
{
    combinator_t *semi          = literal(";");
    combinator_t *forward_slash = literal("/");

    combinator_t *com_semi_parameter       = sequence(2, WRAP(COMBINATOR(semi), PARSER(parameter)));
    combinator_t *com_many_semi_parameters = many(WRAP(COMBINATOR(com_semi_parameter)), 0, -1);

    combinator_t *com_media_type = sequence(4, WRAP(PARSER(type), COMBINATOR(forward_slash), PARSER(subtype), COMBINATOR(com_many_semi_parameters)));

    ssize_t result = COMBINATOR_START(com_media_type, string);
    unused(ctx);

    free_com(com_media_type);
    free_com(com_many_semi_parameters);
    free_com(com_semi_parameter);
    free_com(forward_slash);
    free_com(semi);
    return result;
}

ssize_t allow(const char *string, void *ctx)
{
    combinator_t *literal_allow = literal("Allow");
    combinator_t *colon         = literal(":");

    combinator_t *com_method_list = list(WRAP(PARSER(method)), 1, -1);
    combinator_t *com_many_lws    = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_allow = sequence(4, WRAP(COMBINATOR(literal_allow), COMBINATOR(colon), COMBINATOR(com_many_lws), COMBINATOR(com_method_list)));

    ssize_t result = COMBINATOR_START(com_allow, string);
    unused(ctx);

    free_com(com_allow);
    free_com(com_many_lws);
    free_com(com_method_list);
    free_com(colon);
    free_com(literal_allow);
    return result;
}

ssize_t content_encoding(const char *string, void *ctx)
{
    combinator_t *literal_content_encoding = literal("Content-Encoding");
    combinator_t *colon                    = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_content_encoding = sequence(4, WRAP(COMBINATOR(literal_content_encoding), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(content_coding)));

    ssize_t result = COMBINATOR_START(com_content_encoding, string);
    unused(ctx);

    free_com(com_content_encoding);
    free_com(com_many_lws);
    free_com(colon);
    free_com(literal_content_encoding);
    return result;
}

ssize_t content_length(const char *string, void *ctx)
{
    combinator_t *literal_content_length = literal("Content-Length");
    combinator_t *colon                  = literal(":");
    combinator_t *one_or_more_digits     = many(WRAP(PARSER(digit)), 1, -1);

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_content_length = sequence(4, WRAP(COMBINATOR(literal_content_length), COMBINATOR(colon), COMBINATOR(com_many_lws), COMBINATOR(one_or_more_digits)));

    ssize_t result = COMBINATOR_START(com_content_length, string);
    unused(ctx);

    free_com(com_content_length);
    free_com(com_many_lws);
    free_com(one_or_more_digits);
    free_com(colon);
    free_com(literal_content_length);
    return result;
}

ssize_t content_type(const char *string, void *ctx)
{
    combinator_t *literal_content_type = literal("Content-Type");
    combinator_t *colon                = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_content_type = sequence(4, WRAP(COMBINATOR(literal_content_type), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(media_type)));

    ssize_t result = COMBINATOR_START(com_content_type, string);
    unused(ctx);

    free_com(com_content_type);
    free_com(com_many_lws);
    free_com(colon);
    free_com(literal_content_type);
    return result;
}

ssize_t expires(const char *string, void *ctx)
{
    combinator_t *literal_expires = literal("Expires");
    combinator_t *colon           = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_expires = sequence(4, WRAP(COMBINATOR(literal_expires), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(http_date)));

    ssize_t result = COMBINATOR_START(com_expires, string);
    unused(ctx);

    free_com(com_expires);
    free_com(com_many_lws);
    free_com(colon);
    free_com(literal_expires);
    return result;
}

ssize_t last_modified(const char *string, void *ctx)
{
    combinator_t *literal_last_modified = literal("Last Modified");
    combinator_t *colon                 = literal(":");

    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);

    combinator_t *com_last_modified = sequence(4, WRAP(COMBINATOR(literal_last_modified), COMBINATOR(colon), COMBINATOR(com_many_lws), PARSER(http_date)));

    ssize_t result = COMBINATOR_START(com_last_modified, string);
    unused(ctx);

    free_com(com_last_modified);
    free_com(com_many_lws);
    free_com(colon);
    free_com(literal_last_modified);
    return result;
}

ssize_t general_header(const char *string, void *ctx)
{
    combinator_t *com_many_lws              = many(WRAP(PARSER(lws)), 0, -1);
    combinator_t *com_general_header_choice = choice(2, WRAP(PARSER(date), PARSER(pragma)));
    combinator_t *com_general_header        = sequence(2, WRAP(COMBINATOR(com_many_lws), COMBINATOR(com_general_header_choice)));

    ssize_t result = COMBINATOR_START(com_general_header, string);
    unused(ctx);

    free_com(com_general_header);
    free_com(com_general_header_choice);
    free_com(com_many_lws);
    return result;
}

ssize_t request_header(const char *string, void *ctx)
{
    combinator_t *com_many_lws              = many(WRAP(PARSER(lws)), 0, -1);
    combinator_t *com_request_header_choice = choice(3, WRAP(PARSER(if_modified_since), PARSER(referer), PARSER(user_agent)));
    combinator_t *com_request_header        = sequence(2, WRAP(COMBINATOR(com_many_lws), COMBINATOR(com_request_header_choice)));

    ssize_t result = COMBINATOR_START(com_request_header, string);
    unused(ctx);

    free_com(com_request_header);
    free_com(com_request_header_choice);
    free_com(com_many_lws);
    return result;
}

ssize_t entity_header(const char *string, void *ctx)
{
    combinator_t *com_many_lws = many(WRAP(PARSER(lws)), 0, -1);
    combinator_t *com_entity_header_choice =
        choice(6, WRAP(PARSER(allow), PARSER(content_encoding), PARSER(content_length), PARSER(content_type), PARSER(expires), PARSER(last_modified)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    combinator_t *com_entity_header = sequence(2, WRAP(COMBINATOR(com_many_lws), COMBINATOR(com_entity_header_choice)));

    ssize_t result = COMBINATOR_START(com_entity_header, string);
    unused(ctx);

    free_com(com_entity_header);
    free_com(com_entity_header_choice);
    free_com(com_many_lws);
    return result;
}

ssize_t extension_header(const char *string, void *ctx)
{
    return http_header(string, ctx);
}

ssize_t extension_method(const char *string, void *ctx)
{
    return token(string, ctx);
}

ssize_t method(const char *string, void *ctx)
{
    combinator_t *options = literal("OPTIONS");
    combinator_t *get     = literal("GET");
    combinator_t *head    = literal("HEAD");
    combinator_t *post    = literal("POST");
    combinator_t *put     = literal("PUT");
    combinator_t *delete  = literal("DELETE");
    combinator_t *trace   = literal("TRACE");
    combinator_t *connect = literal("CONNECT");

    combinator_t *com_method = choice(9,    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                                      WRAP(COMBINATOR(options), COMBINATOR(get), COMBINATOR(head), COMBINATOR(post), COMBINATOR(put), COMBINATOR(delete), COMBINATOR(trace), COMBINATOR(connect), PARSER(extension_method)));

    ssize_t result = COMBINATOR_START(com_method, string);
    unused(ctx);

    free_com(com_method);
    free_com(connect);
    free_com(trace);
    free_com(delete);
    free_com(put);
    free_com(post);
    free_com(head);
    free_com(get);
    free_com(options);
    return result;
}

ssize_t request_uri(const char *string, void *ctx)
{
    combinator_t *asterik = literal("*");

    combinator_t *com_request_uri = choice(3, WRAP(COMBINATOR(asterik), PARSER(absolute_uri), PARSER(abs_path)));

    ssize_t result = COMBINATOR_START(com_request_uri, string);
    unused(ctx);

    free_com(com_request_uri);
    free_com(asterik);
    return result;
}

ssize_t http_version(const char *string, void *ctx)
{
    combinator_t *http          = literal("HTTP");
    combinator_t *forward_slash = literal("/");
    combinator_t *dot           = literal(".");
    combinator_t *digit1        = many(WRAP(PARSER(digit)), 1, 1);

    combinator_t *com_http_version = sequence(5, WRAP(COMBINATOR(http), COMBINATOR(forward_slash), COMBINATOR(digit1), COMBINATOR(dot), COMBINATOR(digit1)));    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

    ssize_t result = COMBINATOR_START(com_http_version, string);
    unused(ctx);

    free_com(com_http_version);
    free_com(digit1);
    free_com(dot);
    free_com(forward_slash);
    free_com(http);
    return result;
}
