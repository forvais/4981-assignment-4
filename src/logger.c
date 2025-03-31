#include "logger.h"
#include <stdarg.h>
#include <stdio.h>

static LOG_LEVEL log_level = LOG_LEVEL_INFO;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void logger_log(FILE *stream, const char *format, LOG_LEVEL max_level, va_list args) __attribute__((format(printf, 2, 0)));

void logger_set_level(LOG_LEVEL level)
{
    log_level = level;
}

static void logger_log(FILE *stream, const char *format, LOG_LEVEL max_level, va_list args)
{
    if(log_level < max_level)
    {    // if the current level is higher than the max level, do not emit the message.
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    vfprintf(stream, format, args);    // NOLINT(clang-analyzer-valist.Uninitialized)
#pragma GCC diagnostic pop
}

void log_critical(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_log(stderr, format, LOG_LEVEL_CRITICAL, args);
    va_end(args);
}

void log_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_log(stderr, format, LOG_LEVEL_ERROR, args);
    va_end(args);
}

void log_warn(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_log(stderr, format, LOG_LEVEL_WARN, args);
    va_end(args);
}

void log_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_log(stdout, format, LOG_LEVEL_INFO, args);
    va_end(args);
}

void log_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logger_log(stdout, format, LOG_LEVEL_DEBUG, args);
    va_end(args);
}
