#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <time.h>

#include "io_backend_call.h"
#include "stdio.h"

void client_cmd_open(int sockfd)
{
    uint8_t file_path_len;
    int flag;
    int local_fd;
    char *file_path;
    recv(sockfd, &file_path_len, sizeof(file_path_len), 0);
    printf("file_path_len=%d\n", file_path_len);
    if (file_path_len > 0)
    {
        file_path = (char *)malloc(file_path_len);
        recv(sockfd, file_path, file_path_len, 0);
        printf("file path=%s ", file_path);

        recv(sockfd, &flag, sizeof(flag), 0);
        printf("flag=%d ", flag);

        local_fd = open(file_path, flag);
        printf("file open fd=%d\n", local_fd);
        send(sockfd, &local_fd, sizeof(local_fd), 0);
    }
}

void client_cmd_mmap(int sockfd)
{
    int fd;
    size_t len;
    void *ptr;
    recv(sockfd, &fd, sizeof(fd), 0);
    recv(sockfd, &len, sizeof(len), 0);
    printf("map fd=%d len=%ld ", fd, len);

    ptr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("ptr=%p\n", ptr);
    send(sockfd, &ptr, sizeof(ptr), 0);
}

void client_cmd_ioctl(int sockfd)
{
    int fd, ret;
    int payload;
    unsigned long request;
    recv(sockfd, &fd, sizeof(fd), 0);
    recv(sockfd, &request, sizeof(request), 0);
    recv(sockfd, &payload, sizeof(payload), 0);

    ret = ioctl(fd, request, &payload);

    send(sockfd, &ret, sizeof(ret), 0);
}

void client_cmd_write(io_mapped_device *d, int sockfd)
{
    uint32_t addr;
    uint32_t value;
    recv(sockfd, &addr, sizeof(addr), 0);
    recv(sockfd, &value, sizeof(value), 0);
    io_write_mapped_device(d, addr, value);
}

void client_cmd_read(io_mapped_device *d, int sockfd)
{
    uint32_t addr;
    uint32_t ret;
    recv(sockfd, &addr, sizeof(addr), 0);

    ret = io_read_mapped_device(d, addr);
    // printf("read %d <- %p\n", *addr, addr);
    send(sockfd, &ret, sizeof(ret), 0);
    // *addr = value;
}

void client_cmd_write_burst(int sockfd)
{
    uint32_t *addr;
    uint32_t size;
    int total_size = 0, recv_size = 0, _check = 0;
    recv(sockfd, &addr, sizeof(addr), 0);
    recv(sockfd, &size, sizeof(size), 0);

    total_size = (int)size;
    // printf("busrt write req_len=%d/%d len=%d -> %p ",  size, total_size, recv_size, addr);
    while (total_size > 0)
    {
        recv_size = recv(sockfd, addr, total_size, 0);
        total_size -= recv_size;
        _check += recv_size;
    }
    // printf("%d recieved\n", _check);
    send(sockfd, &recv_size, sizeof(recv_size), 0);
    // *addr = value;
}

void client_cmd_read_burst(int sockfd)
{
    uint32_t *addr;
    uint32_t size, send_size;
    recv(sockfd, &addr, sizeof(addr), 0);
    recv(sockfd, &size, sizeof(size), 0);
    send_size = send(sockfd, addr, size, 0);
    printf("busrt read len=%d <- %p\n", send_size, addr);
    // send(sockfd, &send_size, sizeof(send_size), 0);
    // *addr = value;
}

void client_open_stream(int sockfd)
{
}

int client_mapped_cmd_handle(io_mapped_device *device, int sockfd)
{
    int n;
    io_call_cmd cmd;

    n = read(sockfd, &cmd, sizeof(cmd)); //, 0);
    if (n > 0)
    {
        // printf("client_cmd_call type=%d\n", cmd->call_type);
        switch (cmd.call_type)
        {
        case CALL_WRITE:
            client_cmd_write(device, sockfd);
            break;
        case CALL_READ:
            client_cmd_read(device, sockfd);
            break;
        default:
            break;
        }
    }

    return n;
}

void client_cmd_write_stream(io_stream_device *d, int sockfd)
{
    struct channel_buffer *buffer_test;
    uint32_t stream_size = MAX_SAMPLES * sizeof(iq_buffer);
    uint32_t n, recv_size = 0;
    buffer_test = io_stream_get_buffer(d);

    recv(sockfd, &stream_size, sizeof(stream_size), 0);
    while (recv_size < stream_size)
    {
        n = recv(sockfd, buffer_test->buffer, stream_size, 0);
        if (n <= 0)
            break;
        recv_size += n;
    }
    io_write_stream_device(d, (void *)buffer_test->buffer, stream_size);
    send(sockfd, &recv_size, sizeof(recv_size), 0);
}

struct stream_status
{
    int sockfd;
    uint32_t stream_n;
    uint32_t stop;
    uint64_t accu_size;
};

struct stream_control
{
    int sockfd;
    struct stream_status s;
    uint8_t sync;
};

void *client_write_stream_monitor(void *ptr)
{
    struct stream_status *s = (struct stream_status *)ptr;
    uint64_t last_size;
    printf("Monitoring...\n\n");
    char indicate[4] = "-\\|/";
    int i = 0;
    while (!s->stop)

    {
        last_size = s->accu_size;
        usleep(200e3);
        fprintf(stderr, "\r%c Streaming %u issued.... Speed %4.2fMB/s", indicate[i++ % 4], s->stream_n, 5 * ((float)(s->accu_size - last_size)) / (1024 * 1024));
    }
    printf("\n");
}

int client_cmd_write_stream_inf(io_stream_device *d, int sockfd)
{
    struct channel_buffer *ch_buffer;
    uint32_t stream_size = MAX_SAMPLES * sizeof(iq_buffer);
    uint32_t n, recv_size = 0;
    uint8_t start_cmd;
    uint32_t buffer_ptr = 0;

    int pid;
    pthread_t monitor_thread;

    struct stream_status status =
        {
            .stream_n = 0,
            .accu_size = 0,
            .stop = 0};
    pid = pthread_create(&monitor_thread, NULL, client_write_stream_monitor, &status);
    ch_buffer = io_stream_get_buffer(d);

    while (1)
    {
        recv(sockfd, &start_cmd, sizeof(start_cmd), 0);
        if ((start_cmd >> 1) == CALL_WRITE_STREAM)
        {
            n = recv(sockfd, &recv_size, sizeof(recv_size), 0);
            // printf("Receive stream %d: %u(n=%d)\n", start_cmd, recv_size, n);
            while (recv_size > 0)
            {
                n = recv(sockfd, ((void *)ch_buffer->buffer) + buffer_ptr, recv_size, 0);
                if (n <= 0)
                    break;
                recv_size -= n;
                buffer_ptr += n;
                status.accu_size += n;
                if (buffer_ptr >= stream_size) // Start local stream
                {
                    io_write_stream_device(d, (void *)ch_buffer->buffer, stream_size);
                    ch_buffer = io_stream_get_buffer(d);
                    buffer_ptr = 0;
                    status.stream_n++;
                }
            }
        }
        else
        {
            printf("Failed to sync with write stream %d\n", start_cmd);
        }
        if (send(sockfd, &(uint8_t){1}, 1, 0) == -1) // check if client stop
            break;
    }
    status.stop = 1;
    pthread_join(monitor_thread, NULL);
    printf("%d transferred.\n", recv_size);
    return 0;
}

void *client_read_stream_control(void *ptr)
{
    struct stream_control *ctrl = (struct stream_control *)ptr;
    struct stream_status *s = &ctrl->s;
    uint8_t cmd = 0;
    int n;
    while (1)
    {
        n = recv(s->sockfd, &cmd, sizeof(cmd), 0);
        if (n <= 0)
        {
            s->stop = 1;
            return NULL;
        }
        if (cmd == (CALL_SYNC_STREAM << 1))
        {
            ctrl->sync = 1;
        }
    }
}

int client_cmd_read_stream_inf(io_stream_device *d, int sockfd)
{
    struct channel_buffer *ch_buffer = NULL;
    uint32_t send_size, stream_size = 1024 * sizeof(iq_buffer);
    int n, ret, act = 0;
    uint8_t start_cmd = (CALL_READ_STREAM << 1), recv_cmd;
    uint32_t buffer_ptr = 0;
    struct iovec iov[4];

    int pid;
    pthread_t monitor_thread;

    struct stream_control ctrl =
        {
            .s = {
                .sockfd = sockfd,
                .stream_n = 0,
                .accu_size = 0,
                .stop = 0},
            .sync = 0};

    fd_set rfds, wfds;
    // pid = pthread_create(&monitor_thread, NULL, client_read_stream_control, &ctrl);

    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);

    while (1)
    {

        if (ch_buffer)
        {
            // n = recv(sockfd, &recv_cmd, sizeof(recv_cmd), 0);
            // if (recv_cmd == start_cmd)
            // {
            //     printf("Synced");
            // }
            send_size = stream_size;
            iov[0].iov_base = &start_cmd;
            iov[0].iov_len = sizeof(start_cmd);


            iov[1].iov_base = &send_size;
            iov[1].iov_len = sizeof(send_size);
            iov[2].iov_base = (void *)ch_buffer->buffer;
            iov[2].iov_len = send_size;

            n = writev(sockfd, iov, 3);

            if (n <= 0)
            {
                break;
            }

            ctrl.s.accu_size += n;
            ctrl.s.stream_n += 1;

            io_read_stream_device(d, (void *)ch_buffer->buffer, stream_size);
            ch_buffer = io_stream_get_buffer(d);
        }
        else
        {
            ch_buffer = io_stream_get_buffer(d);
        }
    }

    ctrl.s.stop = 1;
    // pthread_join(monitor_thread, NULL);
    return act;
}

int client_stream_cmd_handle(io_stream_device *device, int sockfd)
{
    int n;
    uint8_t cmd;
    int next_action = 0;
    do
    {
        n = recv(sockfd, &cmd, sizeof(cmd), 0); //, 0);
        if (n > 0)
        {
            switch (cmd >> 1)
            {
            case CALL_WRITE_STREAM:
                next_action = client_cmd_write_stream_inf(device, sockfd);
                break;
            case CALL_READ_STREAM:
                next_action = client_cmd_read_stream_inf(device, sockfd);
                break;
            default:
                break;
            }
        }
        // wait for stream closed
        io_sync_stream_device(device);
    } while (next_action);

    return n;
}