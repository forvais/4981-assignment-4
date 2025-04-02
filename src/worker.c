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

    reset_worker(worker);

    errno       = 0;
    worker->pid = fork();
    if(worker->pid < 0)
    {
        seterr(errno);
    }

    if(worker->pid == 0)
    {    // child
        char buf[1];
        close(pipefd[1]);

        read(pipefd[0], &buf, sizeof(uint8_t));
        close(pipefd[0]);

        return 0;
    }

    // Create path to socket file
    errno       = 0;
    socket_path = make_string("./%d.sock", worker->pid);
    if(socket_path == NULL)
    {
        seterr(errno);
        return -2;
    }

    // Setup domain server to establish communication with worker
    seterr(0);
    worker->fd = dmn_server(socket_path, err);
    if(worker->fd < 0)
    {
        return -3;
    }

    log_debug("\n%sServer | Worker:%s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET);
    log_debug("Worker[PID:%d/FD:%d] spawned.\n", worker->pid, worker->fd);

    close(pipefd[0]);
    write(pipefd[1], "1", 1);
    close(pipefd[1]);

    free(socket_path);    // Do not need the path anymore

    return worker->pid;
}

void stop_worker(worker_t *worker)
{
    if(worker != NULL)
    {
        if(worker->pid > 0)
        {
            kill(worker->pid, SIGINT);
            waitpid(worker->pid, NULL, WNOHANG);
        }

        if(worker->fd > -1)
        {
            close(worker->fd);
        }

        if(worker->client.fd > -1)
        {
            close(worker->client.fd);
        }

        reset_worker(worker);
    }
}

void reset_worker(worker_t *worker)
{
    if(worker != NULL)
    {
        worker->pid            = 0;
        worker->fd             = -1;
        worker->client.fd      = -1;
        worker->client.address = 0;
        worker->client.port    = 0;
    }
}

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

    // Set socket path
    pid = getpid();

    socket_path = make_string("./%d.sock", pid);

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

    pollfds[0].fd     = connfd;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;

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

        if(pollfds[0].revents & (POLLIN))
        {
            if(handle_client_data(pollfds[0].fd) == 0)
            {
                // Trigger POLLHUP because the client has closed the connection.
                pollfds[0].revents |= POLLHUP;
            }
        }

        if(pollfds[0].revents & (POLLERR))
        {
            retval = EXIT_FAILURE;    // NOLINT
            goto cleanup;
        }

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
