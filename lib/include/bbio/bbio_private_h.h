
#ifndef _BBIO_PRIVATE_H
#define _BBIO_PRIVATE_H

#include "bbio_h.h"


int io_append_device(io_context *ctx, io_device *device);
int io_count_devices(io_context *ctx);
#endif