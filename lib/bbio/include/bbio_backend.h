#include "bbio_h.h"

#ifndef _BBIO_BACKEND_H
#define _BBIO_BACKEND_H



// private api level proxy
io_mapped_device *io_open_mapped_local(io_context *ctx, char *file_path, size_t size);
io_stream_device *io_open_stream_local(io_context *ctx, char *file_path);
int io_close_mapped_local(io_context *ctx, io_mapped_device *device);
int io_close_stream_local(io_context *ctx, io_stream_device *device);

// Proxy system call
IO_FD io_open_local(io_context *ctx, char *file, int flag);
int io_close_local(io_context *ctx, IO_FD fd);
void *io_mmap_local(io_context *ctx, void **addr, IO_FD fd, size_t __len);
int io_ioctl_local(io_context *ctx, int __fd, unsigned long __request, void *payload);


uint16_t io_write_busrt_local(io_context *ctx, void *addr, void *data, uint32_t size);

uint16_t io_read_busrt_local(io_context *ctx, void *addr, void *data, uint32_t size);
#endif