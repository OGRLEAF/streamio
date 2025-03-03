#include "bbio_backend.h"

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdint.h>

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

// Focus on Network
enum cmd_call_type
{
    CALL_OPEN = 0,
    CALL_CLOSE = 1,
    // CALL_MMAP,
    // CALL_IOCTL,
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

/*
* op
*   op_type[3:0] 0 = open
*                1 = close
*                2 = write
*                3 = read
*   dev_type[7:4] 0 = context
*                 1 = mapped
*                 2 = stream
*/

#define FRAME_OP_OPEN    (0) 
#define FRAME_OP_CLOS    (1) 
#define FRAME_OP_WRIT    (2)
#define FRAME_OP_READ    (3)

#define FRAME_DEV_CTX    (0 << 4)
#define FRAME_DEV_MAPPED (1 << 4)
#define FRAME_DEV_STREAM (2 << 4)


typedef struct _frame_structure
{
    uint32_t hs; // handshake magic value
    uint8_t op;
} frame_top;

typedef struct _frame_ctx_struct 
{
    struct _frame_structure prefix;
    uint32_t ctx_id;
} frame_ctx_top;

struct _frame_mapped_struct
{
    struct _frame_ctx_struct prefix;
    uint32_t fd;
};

struct _frame_stream_struct
{
    struct _frame_ctx_struct prefix;
    uint32_t fd;
};

#define FRAME(_op)                   { .hs = HANDSHAKE_HEADER, .op = (_op) }

#define FRAME_OPEN_CTX              FRAME(FRAME_OP_OPEN | FRAME_DEV_CTX) 
#define FRAME_CLOSE_CTX             FRAME(FRAME_OP_CLOS | FRAME_DEV_CTX) 

#define FRAME_WITH_CTX(frame, _ctx_id)   {.prefix = frame, .ctx_id = _ctx_id }


#define FRAME_OPEN_MAPPED(ctx_id)          FRAME_WITH_CTX(FRAME(FRAME_OP_OPEN | FRAME_DEV_MAPPED), ctx_id)
#define FRAME_WRITE_MAPPED(ctx_id, _fd)     { .prefix = FRAME_WITH_CTX(FRAME(FRAME_OP_WRIT | FRAME_DEV_MAPPED), ctx_id), fd = _fd }
#define FRAME_READ_MAPPED(ctx_id, _fd)      { .prefix = FRAME_WITH_CTX(FRAME(FRAME_OP_READ | FRAME_DEV_MAPPED), ctx_id), fd = _fd }

#define FRAME_OPEN_STREAM(ctx_id)          FRAME_WITH_CTX(FRAME(FRAME_OP_OPEN | FRAME_DEV_STREAM), ctx_id)
#define FRAME_WRITE_STREAM(ctx_id, _fd)     { .prefix = FRAME_WITH_CTX(FRAME(FRAME_OP_WRIT | FRAME_DEV_STREAM), ctx_id), fd = _fd }
#define FRAME_READ_STREAM(ctx_id, _fd)      { .prefix = FRAME_WITH_CTX(FRAME(FRAME_OP_READ | FRAME_DEV_STREAM), ctx_id), fd = _fd }




#endif
