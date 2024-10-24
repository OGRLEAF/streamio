#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"

#ifndef _BBIO_H

#define _BBIO_H

#define MAX_DEVICE 4

typedef int IO_FD;

typedef struct _bbio_context_base io_context;
typedef struct _bbio_device_base io_device;
typedef struct _bbio_device_mapped_channel io_mapped_channel;
typedef struct _bbio_device_stream_channel io_stream_channel;
typedef struct _bbio_stream_device io_stream_device;
typedef struct _bbio_mapped_device io_mapped_device;

typedef io_mapped_device *(*io_open_mapped_channel_call)(io_context *ctx, char *file_path, size_t size);
typedef io_stream_device *(*io_open_stream_channel_call)(io_context *ctx, char *file_path);
typedef int (*io_close_mapped_call)(io_context *ctx, io_mapped_device *device);
typedef int (*io_close_stream_call)(io_context *ctx, io_stream_device *device);

typedef void (*io_write_call)(io_mapped_device *device, uint32_t addr, uint32_t value);
typedef uint32_t (*io_read_call)(io_mapped_device *device, uint32_t addr);

typedef uint32_t (*io_write_stream)(io_stream_device *device, void *data, uint32_t size);
typedef uint32_t (*io_read_stream)(io_stream_device *device, void *data, uint32_t size);
typedef void (*io_sync_stream)(io_stream_device *device);
typedef struct channel_buffer *(*io_stream_alloc_buffer)(io_stream_device *device, uint32_t flag);
enum IO_DEVICE_TYPE
{
    MAPPED_DEVICE = 0,
    STREAM_DEVICE = 1,
};

struct _bbio_device_mapped_channel
{
    io_write_call write;
    io_read_call read;
    uint32_t *start;
    uint32_t *end;
};

struct _bbio_device_stream_channel
{
    io_write_stream write_stream;
    io_read_stream read_stream;
    io_sync_stream sync_stream;
    io_stream_alloc_buffer alloc_buffer;
    void *private;
};

struct _bbio_device_base
{
    uint16_t id;
    uint16_t type;
    io_context *ctx;
    char *path;
    void *mmap_mirror;
};

typedef struct _bbio_stream_device
{
    io_device device;
    io_stream_channel ch;
    IO_FD fd;
    uint16_t buffer_n;
    // io_stream_buffer * buffer_pool;
    uint16_t current_buffer;
} io_stream_device;

typedef struct _bbio_mapped_device
{
    io_device device;
    io_mapped_channel ch;
    int fd;
} io_mapped_device;

typedef struct _bbio_context_backend
{
    io_open_mapped_channel_call open_mapped;
    io_open_stream_channel_call open_stream;
    io_close_mapped_call close_mapped;
    io_close_stream_call close_stream;
    // io_open_call open;
    // io_close_call close;
    // io_mmap_call mmap;
    // io_ioctl_call ioctl;
    // io_write_call write;
    // io_read_call read;
    // io_write_busrt_call write_burst;
    // io_read_busrt_call read_burst;
    // io_sync_buffer sync_buffer;
} io_backend;

typedef struct _bbio_context_base
{
    io_device **devices;
    io_backend backend;
} io_context;

typedef struct _bbio_stream_buffer
{
    uint32_t id;
    uint32_t size;
    uint8_t busy;
    char *buffer;
} io_stream_buffer;

typedef struct _bbio_stream_buffer_task_
{
    struct channel_buffer *channel_buffer;
    int buffer_id;
} io_stream_task;

io_context *io_create_context();
void io_close_context(io_context *ctx);
io_context *io_create_net_context(char *host, uint16_t port);

io_device *io_add_mapped_device(io_context *ctx, char *path);
io_device *io_add_stream_device(io_context *ctx, char *path);

uint32_t io_read_mapped_device(io_mapped_device *device, uint32_t addr);
void io_write_mapped_device(io_mapped_device *device, uint32_t addr, uint32_t value);

struct channel_buffer *io_stream_get_buffer(io_stream_device *device);
void io_write_stream_device(io_stream_device *device, void *data, uint32_t size);
void io_read_stream_device(io_stream_device *device, void *data, uint32_t size);

void io_sync_stream_device(io_stream_device *device);
#endif