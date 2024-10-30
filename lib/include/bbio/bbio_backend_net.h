#include "bbio_backend.h"

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pthread.h>

#ifndef _BBIO_BACKEND_NET
#define _BBIO_BACKEND_NET
#define HANDSHAKE_HEADER (0x114514)

typedef struct _handshake
{
    uint32_t head;
    uint8_t type; // mapped=0, stream=1
} handshake_msg;


enum cmd_channel_type
{
    MAPPED_CHANNEL = 0,
    STREAM_CHANNEL = 1
};

enum cmd_call_type
{
    CALL_OPEN = 0,
    CALL_CLOSE = 1,
    CALL_MMAP,
    CALL_IOCTL,
    CALL_WRITE,
    CALL_READ,
    CALL_WRITE_BURST,
    CALL_READ_BURST,
    CALL_WRITE_STREAM,
    CALL_READ_STREAM,
    CALL_SYNC_STREAM
};

typedef struct _bbio_net_context
{
    io_context context;
    struct sockaddr_in serv_addr;
    int sockfd;
    pthread_mutex_t lock;
} io_net_context;


typedef struct _stream_read_net_option {
    uint32_t packet_size;
} io_stream_read_net_option;

io_mapped_device *io_open_mapped_net(io_context *ctx, char *file_path, size_t size);
io_stream_device *io_open_stream_net(io_context *ctx, char *file_path);
int io_close_stream_net(io_context *ctx, io_stream_device *device);
int io_close_mapped_net(io_context *ctx, io_mapped_device *device);
#endif