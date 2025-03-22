#include "logger.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) (void)(x)
#define UNKNOWN_OPTION_MESSAGE_LEN 22

typedef struct
{
    bool debug;
} arguments_t;

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message);
static void           get_arguments(arguments_t *args, int argc, char *argv[]);
static void           validate_arguments(const char *binary_name, arguments_t *args);

int main(int argc, char *argv[])
{
    arguments_t args;

    // Get arguments
    memset(&args, 0, sizeof(arguments_t));
    get_arguments(&args, argc, argv);
    validate_arguments(argv[0], &args);

    // Set logger levels
    logger_set_level(args.debug ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
    log_debug("Running in DEBUG mode.\n\n");

    // Do stuff
    log_info("Hello, world!\n");

    // Done!
    return EXIT_SUCCESS;
}

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] [-d]\n", binary_name);
    fputs("Options:\n", stderr);
    fputs("  -h, --help    Display this help message\n", stderr);
    fputs("  -d, --debug   Enables the debug mode\n", stderr);
    exit(exit_code);
}

static void get_arguments(arguments_t *args, int argc, char *argv[])
{
    int opt;

    static struct option long_options[] = {
        {"debug", optional_argument, NULL, 'd'},
        {"help",  no_argument,       NULL, 'h'},
        {NULL,    0,                 NULL, 0  }
    };

    while((opt = getopt_long(argc, argv, "hd::", long_options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'd':
                args->debug = (optarg == NULL);
                break;
            case 'h':
                usage(argv[0], EXIT_SUCCESS, NULL);
            case '?':
                if(optopt != 'd')
                {
                    char message[UNKNOWN_OPTION_MESSAGE_LEN];

                    snprintf(message, sizeof(message), "Unknown option '-%c'.", optopt);
                    usage(argv[0], EXIT_FAILURE, message);
                }
                break;
            default:
                usage(argv[0], EXIT_FAILURE, NULL);
        }
    }
}

static void validate_arguments(const char *binary_name, arguments_t *args)
{
    UNUSED(binary_name);
    UNUSED(args);
}
