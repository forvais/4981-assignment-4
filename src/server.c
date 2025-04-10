#include "handlers.h"
#include "loader.h"
#include "logger.h"
#include "networking.h"
#include "state.h"
#include "utils.h"
#include "worker.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

#define UNKNOWN_OPTION_MESSAGE_LEN 22
#define POLL_TIMEOUT (-1)
#define MAX_CLIENTS 1024
#define PUBLIC_DIR "./public/"

typedef struct
{
    char       *address;
    in_port_t   port;
    bool        debug;
    const char *libhttp_path;
    size_t      workers;
    const char *public_dir;
} arguments_t;

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message);
static void           get_arguments(arguments_t *args, int argc, char *argv[]);
static void           validate_arguments(const char *binary_name, arguments_t *args);

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

    int inotify_fd;
    int inotify_w;

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

    // Setup library loading
    inotify_fd = inotify_init1(IN_CLOEXEC | O_NONBLOCK);
    if(inotify_fd < 0)
    {
        return EXIT_FAILURE;
    }

    errno     = 0;
    inotify_w = inotify_add_watch(inotify_fd, ".", IN_CREATE | IN_MODIFY | IN_DELETE);
    if(inotify_w < 0)
    {
        return EXIT_FAILURE;
    }

    if(reload_library(args.libhttp_path) < 0)
    {
        log_error("main::reload_library: %s\n", dlerror());
    }

    // Set number of available workers
    app_set_desired_workers(&app, args.workers, NULL);

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

        // Check for library updates
        check_library_update(inotify_fd, args.libhttp_path, NULL);

        // Scale workers
        app_health_check_workers(&app, NULL);
        if(app_scale_workers(&app, args.public_dir, &err) < 0)
        {
            log_error("main::app_scale_workers: Failed to scale workers (%s)\n", strerror(err));
        }

        // Listen for events
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
        {    // On client connect...
            // Accept the client and assign the client to a worker...
            handle_client_connect(app.pollfds[0].fd, &app, args.libhttp_path);
        }

        // Iterate through all workers
        for(size_t idx = 0; idx < app.nworkers; idx++)
        {
            struct pollfd *worker_pollfd = &app.pollfds[1 + idx];
            worker_t      *worker        = app_find_worker_by_fd(&app, worker_pollfd->fd);

            int status;

            if(worker == NULL || worker->pid == 0 || worker->fd < 0 || worker->client.fd < 0)
            {
                continue;    // Only allow workers with assigned clients
            }

            if(waitpid(worker->pid, &status, WNOHANG) != 0 && WIFEXITED(status))
            {
                worker_pollfd->revents |= POLLHUP;
            }

            if(worker_pollfd->revents & POLLIN)
            {
                handle_worker_connect(worker, worker->fd);
            }

            if(worker_pollfd->revents & (POLLHUP | POLLERR))
            {
                handle_worker_disconnect(worker, &app);
            }
        }
    }

    close(sockfd);
    inotify_rm_watch(inotify_fd, inotify_w);
    close(inotify_fd);

    for(size_t idx = 0; idx < app.nworkers; idx++)
    {
        const worker_t *worker = &app.workers[idx];

        if(app_remove_worker(&app, worker->pid, NULL) < 0)
        {
            log_error("main::app_remove_worker: Failed to remove worker.\n");
        }
    }

    app_destroy(&app, NULL);

    // Done!
    return EXIT_SUCCESS;
}

static _Noreturn void usage(const char *binary_name, int exit_code, const char *message)
{
    if(message)
    {
        fprintf(stderr, "%s\n\n", message);
    }

    fprintf(stderr, "Usage: %s [-h] [-d] [-l <filepath>] [-w <workers>] -a <address> -p <port>\n", binary_name);
    fputs("Options:\n", stderr);
    fputs("  -a, --address <address>   Address of the web server\n", stderr);
    fputs("  -p, --port <port>         Port to bind to\n", stderr);
    fputs("  -h, --help                Display this help message\n", stderr);
    fputs("  -d, --debug               Enables the debug mode\n", stderr);
    fputs("  -l, --lib <filepath>      Filepath to an accompanying HTTP library.\n", stderr);
    fputs("  -w, --workers <workers>   Number of workers to always be available.\n", stderr);
    fputs("  -s, --serve <directory>   Serve files from inside this directory.\n", stderr);
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
        {"lib",     required_argument, NULL, 'l'},
        {"workers", required_argument, NULL, 'w'},
        {"serve",   required_argument, NULL, 's'},
        {"help",    no_argument,       NULL, 'h'},
        {NULL,      0,                 NULL, 0  }
    };

    while((opt = getopt_long(argc, argv, "hda:p:l:w:s:", long_options, NULL)) != -1)
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
            case 'l':
                args->libhttp_path = optarg;
                break;
            case 'w':
                if(optarg)
                {
                    char *end;

                    args->workers = strtoul(optarg, &end, 10);    // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                }
                break;
            case 's':
                args->public_dir = optarg;
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

static void validate_arguments(const char *binary_name, arguments_t *args)
{
    if(args->address == NULL)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an IPv4 or IPv6 address to connect to.");
    }

    if(args->port == 0)
    {
        usage(binary_name, EXIT_FAILURE, "You must provide an available port to connect to.");
    }

    if(args->libhttp_path == NULL)
    {
        args->libhttp_path = LIBHTTP_PATH;
    }

    if(args->workers == 0)
    {
        args->workers = NUM_WORKERS;
    }

    if(args->public_dir == NULL)
    {
        args->public_dir = PUBLIC_DIR;
    }
}
