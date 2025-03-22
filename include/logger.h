#ifndef LOGGER_H
#define LOGGER_H

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

typedef enum
{
    LOG_LEVEL_CRITICAL,    // Critial failure, application will soon shutdown for safety
    LOG_LEVEL_ERROR,       // Operation failed
    LOG_LEVEL_WARN,        // Potentionally leads to issues later on
    LOG_LEVEL_INFO,        // General informational messages
    LOG_LEVEL_DEBUG,       // Includes additional information for debugging purposes
} LOG_LEVEL;

void logger_set_level(LOG_LEVEL level);

void log_critical(const char *format, ...);
void log_error(const char *format, ...);
void log_warn(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);

#endif /* LOGGER_H */
