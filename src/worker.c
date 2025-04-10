#include "worker.h"
#include "handlers.h"
#include "io.h"
#include "logger.h"
#include "networking.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool volatile is_running = true;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/*
 * Spawns a worker and setups up a domain socket to communicate with the worker.
 *
 * When the worker spawns, it will try to connect with the domain socket as soon as possible.
 *
 * A race condition occurs where the domain socket can't be established before the workers tries to connect,
 * causing a connection error.
 */
int spawn_worker(worker_t *worker, int *err)
{
    char *socket_path;

    int pipefd[2];

    // Create a pipe to block the child process until the main process is ready
    errno = 0;
    if(pipe(pipefd) < 0)    // NOLINT(android-cloexec-pipe)
    {
        log_error("spawn_worker::pipe: %s\n", strerror(errno));
        return -1;
    }

    // Set worker to default values
    if(reset_worker(worker, err) < 0)
    {
        log_error("spawn_worker::reset_worker: Failed to set default values on worker\n");
        return -2;
    }

    // Fork and set worker pid
    errno       = 0;
    worker->pid = fork();
    if(worker->pid < 0)
    {
        seterr(errno);
    }

    // Check if pid is worker
    if(worker->pid == 0)
    {    // child
        char buf[1];
        close(pipefd[1]);

        // Block to wait for continue signal from parent
        read(pipefd[0], &buf, sizeof(uint8_t));
        close(pipefd[0]);

        return 0;
    }

    // Create path string to socket file
    errno       = 0;
    socket_path = make_string("./%d.sock", worker->pid);
    if(socket_path == NULL)
    {
        seterr(errno);
        return -3;
    }

    // Setup domain server to establish communication with worker
    seterr(0);
    worker->fd = dmn_server(socket_path, err);
    if(worker->fd < 0)
    {
        return -4;
    }

    free(socket_path);    // Not used anymore

    // Notify the worker has spawned
    log_debug("\n%sServer | Worker:%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
    log_debug("Worker[PID:%d/FD:%d] spawned.\n", worker->pid, worker->fd);

    // Signal the child that it can now stop blocking and continue execution
    close(pipefd[0]);
    write(pipefd[1], "1", 1);
    close(pipefd[1]);

    return worker->pid;
}

/*
 * Signal the worker.
 */
int signal_worker(const worker_t *worker, int signal, int *err)
{
    seterr(0);
    if(worker == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    if(worker->pid > 0)
    {
        int status;
        kill(worker->pid, signal);
        if(waitpid(worker->pid, &status, WNOHANG) != 0 && WIFEXITED(status))    // Clean up potential zombified process
        {
            if(worker->fd > -1)
            {
                close(worker->fd);
            }

            if(worker->client.fd > -1)
            {
                close(worker->client.fd);
            }
        }
    }

    return 0;
}

/*
 * Set worker to default values.
 */
int reset_worker(worker_t *worker, int *err)
{
    seterr(0);
    if(worker == NULL)
    {
        seterr(EINVAL);
        return -1;
    }

    worker->pid            = 0;
    worker->fd             = -1;
    worker->client.fd      = -1;
    worker->client.address = 0;
    worker->client.port    = 0;

    return 0;
}

/*
 * Assign the client to the worker
 */
int assign_client_to_worker(worker_t *worker, const client_t *client, int *err)
{
    if(worker->client.fd > -1)
    {
        *err = EBUSY;
        return -1;    // Worker already has a client, this action overwrite the existing client, leaving it in limbo
    }

    if(!client)
    {
        *err = EINVAL;
        return -2;
    }

    memcpy(&worker->client, client, sizeof(client_t));

    return 0;
}

static void signal_handler_fn(int signal)
{
    if(signal == SIGINT)
    {
        is_running = false;
    }
}

_Noreturn void worker_entrypoint(void)
{
    int retval;
    int err;

    pid_t         pid;
    const char   *socket_path;
    struct pollfd pollfds[1];

    int sockfd = -1;
    int connfd = -1;

    setup_signals(signal_handler_fn);

    // Close sockfd from parent
    close(3);

    // Set socket path
    pid = getpid();

    // Create path string to socket file
    socket_path = make_string("./%d.sock", pid);
    if(socket_path == NULL)
    {
        retval = EXIT_FAILURE;
        goto exit;
    }

    // Open domain socket
    err    = 0;
    sockfd = dmn_client(socket_path, &err);
    if(sockfd < 0)
    {
        if(err == EACCES)
        {
            log_error("worker::dmn_client: \"%s\" %s \n", socket_path, strerror(err));
        }
        else
        {
            log_error("worker::dmn_client: %s\n", strerror(err));
        }

        retval = EXIT_FAILURE;
        goto exit;
    }

    // Get the client fd from the server
    err    = 0;
    connfd = recv_fd(sockfd, &err);
    if(connfd < 0)
    {
        if(err == EINTR)
        {
            retval = EXIT_SUCCESS;
        }
        else
        {
            log_error("worker::recv_fd: %s\n", strerror(err));
            retval = EXIT_FAILURE;
        }

        close(sockfd);
        goto exit;
    }

    // Setup client pollfd
    pollfds[0].fd     = connfd;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;

    // Process data!
    while(is_running)
    {
        int poll_result;

        errno       = 0;
        poll_result = poll(pollfds, ((nfds_t)(sizeof(pollfds) / sizeof(pollfds[0]))), -1);
        if(poll_result < 0)
        {
            log_error("worker::poll: %s\n", strerror(errno));
            continue;
        }

        // On CLIENT data in...
        if(pollfds[0].revents & (POLLIN))
        {
            if(handle_client_data(pollfds[0].fd) == 0)
            {
                // Trigger POLLHUP because the client has closed the connection.
                pollfds[0].revents |= POLLHUP;
            }
        }

        // On CLIENT error...
        if(pollfds[0].revents & (POLLERR))
        {
            retval = EXIT_FAILURE;    // NOLINT
            goto cleanup;
        }

        // On CLIENT shutdown...
        if(pollfds[0].revents & (POLLHUP))
        {
            retval = EXIT_SUCCESS;    // NOLINT
            goto cleanup;
        }
    }

    retval = EXIT_FAILURE;

cleanup:
    if(connfd >= 0)
    {
        close(connfd);
    }

exit:
    exit(retval);
}
