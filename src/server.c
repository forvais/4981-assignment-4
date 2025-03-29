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
    char     *address;
    in_port_t port;
    bool      debug;
} arguments_t;

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message);
static void           get_arguments(arguments_t *args, int argc, char *argv[]);
static void           validate_arguments(const char *binary_name, const arguments_t *args);

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

    fprintf(stderr, "Usage: %s [-h] [-d] -a <address> -p <port>\n", binary_name);
    fputs("Options:\n", stderr);
    fputs("  -a, --address <address>   Address of the web server\n", stderr);
    fputs("  -p, --port <port>         Port to bind to\n", stderr);
    fputs("  -h, --help                Display this help message\n", stderr);
    fputs("  -d, --debug               Enables the debug mode\n", stderr);
    exit(exit_code);
}

static void get_arguments(arguments_t *args, int argc, char *argv[])
{
    int err;
    int opt;

    static struct option long_options[] = {
        {"address", required_argument, NULL, 'a'},
        {"port",    required_argument, NULL, 'p'},
        {"debug",   no_argument,       NULL, 'd'},
        {"help",    no_argument,       NULL, 'h'},
        {NULL,      0,                 NULL, 0  }
    };

    while((opt = getopt_long(argc, argv, "hda:p:", long_options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'a':
                args->address = optarg;
                break;
            case 'p':
                args->port = convert_port(optarg, &err);

                if(err != 0)
                {
                    usage(argv[0], EXIT_FAILURE, "Port must be between 1 and 65535");
                }
                break;
            case 'd':
                args->debug = (optarg == NULL);
                break;
            case 'h':
                usage(argv[0], EXIT_SUCCESS, NULL);
            case '?':
                if(optopt != 'a' && optopt != 'p' && optopt != 'd')
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

static void validate_arguments(const char *binary_name, const arguments_t *args)
{
    if(args->address == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an IPv4 or IPv6 address to connect to.");
    }

    if(args->port == 0)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an available port to connect to.");
    }
}
