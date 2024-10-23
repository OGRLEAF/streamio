#include "buffer.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
struct channel_buffer_context * buffer_allocate(int buffer_count, int channel_fd)
{
    int buffer_id = 0;
    struct channel_buffer * channel_buffer = (struct channel_buffer *)mmap(NULL, sizeof(struct channel_buffer) * buffer_count,
                                                            PROT_READ | PROT_WRITE, MAP_SHARED, channel_fd, 0);
    printf("Buffer allocated\n");
    if(channel_buffer == MAP_FAILED)
    {
        return NULL;
    }
    struct channel_buffer_context * context = (struct channel_buffer_context *) malloc(sizeof(struct channel_buffer_context));
    context->channel_buffer = channel_buffer;
    return context;
}


void buffer_release(struct channel_buffer_context * buffer_context)
{
    int ret = munmap(buffer_context->channel_buffer, sizeof(struct channel_buffer));
    if(ret == -1)
    {
        printf("Failed to unmap buffer\n");
    }else {
        printf("Buffer unmapped\n");
    }
    free(buffer_context);
}

void buffer_configure(struct channel_buffer_context * buffer_context, buffer_on_fill * on_fill)
{
    buffer_context->on_fill = on_fill;
}
