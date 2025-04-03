#include "handlers.h"
#include "logger.h"
#include "networking.h"
#include "state.h"
#include "utils.h"
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNKNOWN_OPTION_MESSAGE_LEN 22
#define POLL_TIMEOUT (-1)
#define MAX_CLIENTS 1024

typedef struct
{
    char     *address;
    in_port_t port;
    bool      debug;
} arguments_t;

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message);
static void           get_arguments(arguments_t *args, int argc, char *argv[]);
static void           validate_arguments(const char *binary_name, const arguments_t *args);

static bool volatile is_running = true;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void signal_handler_fn(int signal)
{
    if(signal == SIGINT)
    {
        is_running = false;
    }
}

int main(int argc, char *argv[])
{
    int err;

    int sockfd;

    app_state_t app;
    arguments_t args;

    setup_signals(signal_handler_fn);

    // Get arguments
    memset(&args, 0, sizeof(arguments_t));
    get_arguments(&args, argc, argv);
    validate_arguments(argv[0], &args);

    // Set logger levels
    logger_set_level(args.debug ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
    log_debug("Running in DEBUG mode.\n\n");

    // Setup app state
    err = 0;
    if(app_init(&app, MAX_CLIENTS, &err) < 0)
    {
        log_error("main::app_init: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // Setup TCP Server
    sockfd = tcp_server(args.address, args.port);
    if(sockfd < -1)
    {
        return EXIT_FAILURE;
    }
    log_info("Listening on %s:%d.\n", args.address, args.port);

    // Add SOCKFD to poll list
    app.pollfds[0].fd     = sockfd;
    app.pollfds[0].events = POLLIN;
    app.npollfds++;
    log_debug("\n%sServer | Init%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
    log_debug("Added server socket to poll list.\n");

    // Poll for connections
    log_debug("Polling for data...\n");
    while(is_running)
    {
        int poll_result;

        errno       = 0;
        poll_result = poll(app.pollfds, (nfds_t)app.npollfds, POLL_TIMEOUT);
        if(poll_result < 0)
        {
            if(errno != EINTR)
            {
                log_error("main::poll: %s\n", strerror(errno));
            }
        }

        // Check incoming connections to server
        if(app.pollfds[0].revents & POLLIN)
        {
            handle_client_connect(app.pollfds[0].fd, &app);
        }

        for(size_t idx = 1; idx < (app.nclients + 1); idx++)
        {
            struct pollfd *client_pollfd = &app.pollfds[idx];

            if(client_pollfd->fd == -1)
            {
                continue;    // Skip invalid file descriptors
            }

            // Handle client requests
            if(client_pollfd->revents & POLLIN)
            {
                if(handle_client_data(client_pollfd->fd) == 0)
                {
                    // Trigger POLLHUP because the client has closed the connection.
                    client_pollfd->revents |= POLLHUP;
                }
            }

            // Handle client disconnects
            if(client_pollfd->revents & (POLLHUP | POLLERR))
            {
                // worker disconnect signal
                handle_client_disconnect(client_pollfd->fd, &app);
            }
        }
    }

    close(sockfd);
    app_destroy(&app);

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
