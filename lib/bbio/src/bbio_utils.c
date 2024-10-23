#include "bbio_private_h.h"
#include <string.h>

int io_count_devices(io_context *ctx)
{
    int i = 0;
    io_device **dev = ctx->devices;
    while (dev[i])
    {
        i++;
    }
    return i;
}

int io_append_device(io_context *ctx, io_device *device)
{
    int current_devices = io_count_devices(ctx);
    io_device **new_devices = (io_device **)malloc(sizeof(io_device *) * (current_devices + 2));
    memset(new_devices, 0, sizeof(io_device *) * (current_devices + 2));
    int i;
    printf("Current devices: %d\n", current_devices);
    if (current_devices < MAX_DEVICE)
    {
        for (i = 0; i < current_devices; i++)
        {
            new_devices[i] = ctx->devices[i];
        }
        new_devices[i] = device;
        new_devices[i + 1] = NULL;
        device->ctx = ctx;
        free(ctx->devices);
        ctx->devices = new_devices;

        return i + 1;
    }

    return -1;
}