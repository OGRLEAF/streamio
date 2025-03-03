#include "io_backend_call.h"
#include "thpool.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <argp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <linux/types.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <signal.h>
#include <net/if.h>
#include <bbio_h.h>

#include "io_backend.h"

typedef struct _run_options
{
    struct sockaddr_in cmd_serv_addr;
    char *dev_path;
} server_options;

typedef struct _cmd_rw
{
    uint16_t rw : 1;   // R = 0; W = 1
    uint16_t type : 1; // REG = 0;  STREAM = 1
    uint16_t addr : 14;
    uint32_t len;
} sys_rw_cmd;

// some global variables
int stop = 0;
int global_sockfd;

static int parse_opt(int key, char *arg, struct argp_state *state)
{
    server_options *options = state->input;
    int input_port = 0;
    switch (key)
    {
    case 'l':
    {
        if (inet_pton(AF_INET, arg, &(options->cmd_serv_addr.sin_addr)))
        {
            printf("Listen to %s\n", arg);
        }
        else
            argp_failure(state, 1, 0, "Invalid addr %s", arg);
        break;
    }
    case 'p':
        input_port = atoi(arg);
        if (input_port <= 65535 && input_port > 0)
            options->cmd_serv_addr.sin_port = htons(atoi(arg));
        else
            argp_failure(state, 1, 0, "Invalid input port %d", input_port);
        break;
    case 'd':
        options->dev_path = arg;
    }

    return 0;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

// void client_info(struct sockaddr_in *cli_addr, char *msg, ...)
// {
//     va_list va_list;
//     char addr_str[INET_ADDRSTRLEN];
//     char *output_buf;
//     char *output_fmt;

//     va_start(va_list, msg);
//     inet_ntop(cli_addr->sin_family,
//               &cli_addr->sin_addr,
//               addr_str, sizeof addr_str);
//     asprintf(&output_fmt, "INFO [%s:%d]%s", addr_str, cli_addr->sin_port, msg);
//     vasprintf(&output_buf, output_fmt, va_list);
// }

int client_finish_handshake(int sockfd) // TODO: io_context
{
    uint32_t ack = HANDSHAKE_HEADER;
    return send(sockfd, &ack, sizeof(ack), 0); // back
}

static io_open_option client_deivce_option(int sockfd)
{
    io_open_option option;
    IO_FD fd;
    uint32_t path_len;
    size_t map_size;
    char *dev_path;

    recv(sockfd, &path_len, sizeof(path_len), 0);
    dev_path = (char *)malloc(path_len);
    recv(sockfd, dev_path, path_len, 0);

    recv(sockfd, &map_size, sizeof(map_size), 0);
    printf("Path len=%d path=%s map_size=%ld\n", path_len, dev_path, map_size);

    option.file_path = dev_path;
    option.size = map_size;

    return option;
}

static int client_mapped_channel(int sockfd)
{
    io_open_option option;
    io_context *ctx = io_create_context();
    io_mapped_device *device;
    if (!client_finish_handshake(sockfd))
        return -1;
    option = client_deivce_option(sockfd);
    printf("Device open: %s %ld\n", option.file_path, option.size);

    device = (io_mapped_device *)io_add_mapped_device(ctx, option.file_path);

    if (device)
    {
        send(sockfd, &device->fd, sizeof(device->fd), 0);
        while (!stop && client_mapped_cmd_handle(device, sockfd))
            ;
    }
    io_close_context(ctx);
    free(option.file_path);
    return 0;
}

static int client_stream_channel(int sockfd)
{
    io_open_option option;
    io_context *ctx = io_create_context();
    io_stream_device *device;
    const int flag = 1;
    if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0 ) {
        error("Failed to set TCP_NODELAY for stream channel");
    }
    // setsockopt(device->fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));

    if (!client_finish_handshake(sockfd))
        return -1;
    option = client_deivce_option(sockfd);
    printf("Stream device open: %s %ld\n", option.file_path, option.size);

    device = (io_stream_device *)io_add_stream_device(ctx, option.file_path);
    if (device)
    {
        send(sockfd, &device->fd, sizeof(device->fd), 0);
        client_stream_cmd_handle(device, sockfd);
    }
    printf("client_stream_channel closed\n");
    io_close_context(ctx);
    free(option.file_path);
    return 0;
}


io_context  *opened_ctx_list[MAX_CONTEXT_COUNT] = {NULL, NULL, NULL, NULL, NULL};



int handle_context(int client_fd, frame_top * frame)
{
    io_context * this_ctx = NULL;
    int i;
    int ret;
    if((frame->op & 0xf) == FRAME_OP_OPEN)
    {
        for(i=0;i<4;i++){
            if(opened_ctx_list[i] == NULL) {
                this_ctx = io_create_context();
                frame_ctx_top ctx_ret = FRAME_WITH_CTX(FRAME(FRAME_OP_OPEN | FRAME_DEV_CTX), i);
                
                ret = send(client_fd, &ctx_ret, sizeof(ctx_ret), 0);
                if(ret < 0)
                {
                    printf("Failed to follow the connection %d\n", client_fd);
                    io_close_context(this_ctx);
                    opened_ctx_list[i] = NULL;
                    return ret;
                }
                else 
                {
                    printf("New context created id=%d\n", i);
                    opened_ctx_list[i] = this_ctx;
                    return 0;
                }
                return ret;
            }

        }
    }
    else if((frame->op & 0xf) == FRAME_OP_CLOS)
    {
        frame_ctx_top client_frame;
        // ret = recv(client_fd, ((void *)(&client_frame.prefix + 1)), (sizeof(frame_ctx_top) - sizeof(frame_top)), 0);
        ret = recv(client_fd, (void *)(&client_frame), sizeof(frame_ctx_top), 0);
        if(ret < 0)
        {
        }
        else
        {
            if(client_frame.ctx_id < MAX_CONTEXT_COUNT)
            {
                if(opened_ctx_list[client_frame.ctx_id] != NULL)
                {
                    this_ctx = opened_ctx_list[client_frame.ctx_id];
                    io_close_context(this_ctx);
                    return 0; 
                }
                else
                {
                    return -1;
                }
            }
        }
        
        
    }
    return -1;
}

int handle_mapped(int clientfd, frame_top *frame)
{

    return -1;
}


void start_task(void * arg)
{
    int ret;
    int client_sockfd = (int) arg;
    frame_top frame_top;
    
    ret = recv(client_sockfd, &frame_top, sizeof(frame_top), MSG_PEEK);

    printf("Task created\n");
    if(ret < 0)
    {
        printf("Failed to initiate task recv %d\n", ret);
        return;
    }


    if((frame_top.op & 0xf0) == FRAME_DEV_CTX)
    {
        while(handle_context(client_sockfd, &frame_top) >= 0)
        {
            ret = recv(client_sockfd, &frame_top, sizeof(frame_top), 0);
            if(ret < 0) {
                printf("Connection disconnected with %d\n", ret);
                return;
            }
            else if(ret == 0)
            {
                printf("Connection disconnected by client\n");
                return;
            }
            else
            {
                printf("Maintain this connection %d\n", ret);
            }
        }
                    
    }
    else if((frame_top.op & 0xf0) == FRAME_DEV_MAPPED)
    {
    }
   return; 
}

frame_handler frame_handlers[] = {
    [0] = handle_context
};

void sigint(int a)
{
    printf("\nStop transmit...\n");
    stop = 1;
    close(global_sockfd);
}

static int server_main(server_options *options)
{
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct ifreq ifr;
    int ret;
    char addr_str[INET_ADDRSTRLEN];

    threadpool thpool = thpool_init(4);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");
    global_sockfd = sockfd;

    if (sockfd < 0)
    {
        error("Failed to open socket\n");
        exit(1);
    }

    serv_addr = options->cmd_serv_addr;

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

    ret = ioctl(sockfd, SIOCGIFADDR, &ifr);

    if (!ret)
    {
        printf("    server ip: %s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    }

    // bzero((char *)&serv_addr, sizeof(serv_addr));

    clilen = sizeof(cli_addr);
    while (!stop)
    {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            error("error when accept");
            close(newsockfd);
            continue;
        }

        inet_ntop(cli_addr.sin_family,
                  &cli_addr.sin_addr,
                  addr_str, sizeof addr_str);
        printf("Server: got connection from %s\n", addr_str);
        

        thpool_add_work(thpool, start_task, (void *)(uintptr_t) newsockfd);
        printf("Thread created\n");
        

        
        // Create process for every context

        /*
        if (!fork())
        {
            close(sockfd);
            global_sockfd = newsockfd;

            recv(newsockfd, &frame, sizeof(frame_top), 0);
            // Use client sockfd to identify context
            if (frame.hs == HANDSHAKE_HEADER)
            {
                switch(frame.op & 0xf0) {
                    case FRAME_DEV_CTX: {
                        break;
                    }
                }
                if (frame.op == MAPPED_CHANNEL)
                    client_mapped_channel(newsockfd);
                else if (hs.type == STREAM_CHANNEL)
                    client_stream_channel(newsockfd);
                else
                    printf("Disconnect with %s, unknown type=%d\n", addr_str, hs.type);
            }
            else
                printf("Disconnect with %s, handshake failed\n", addr_str);
            // TODO free(ctx)
            close(newsockfd);
            exit(0);
        }
        close(newsockfd); // parent 不需要这个
        */
    }

    thpool_wait(thpool);
    thpool_destroy(thpool);


    return 0;
}

int main(int argc, char **argv)
{
    char addr_buf[INET_ADDRSTRLEN];

    struct argp_option options[] =
        {
            {"listen", 'l', "ADDR", 0, "Address to listen, default 0.0.0.0"},
            {"port", 'p', "PORT", 0, "Port to listen, default 13456"},
            {"device", 'd', "DEVICE", 0, "Baseband sys axi device"},
            {0}};
    struct argp argp = {options, parse_opt, 0, 0};
    server_options s_options = {
        .cmd_serv_addr = {
            .sin_addr = INADDR_ANY,
            .sin_port = htons(12345),
            .sin_family = AF_INET,
        },
        .dev_path = "/dev/tc"};
    int ret = argp_parse(&argp, argc, argv, 0, 0, &s_options);

    if (ret)
    {
        exit(ret);
    }
    inet_ntop(AF_INET, &s_options.cmd_serv_addr.sin_addr, addr_buf, INET_ADDRSTRLEN);
    printf("Server will listen on %s:%d\n", addr_buf, ntohs(s_options.cmd_serv_addr.sin_port));

    signal(SIGINT, sigint);
    server_main(&s_options);
    return ret;
}
