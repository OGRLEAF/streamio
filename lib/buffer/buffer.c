#include "buffer.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
struct channel_buffer_context * buffer_allocate(int buffer_count, int channel_fd)
{
    int buffer_id = 0;
    if(buffer_count > BUFFER_COUNT) {
        printf("Request buffer count %d violate max buffer count allowed in driver %d\n", buffer_count, BUFFER_COUNT);
        return NULL;
    }
    /* struct buffer_mmap_space * mmap_space_alloc = (struct buffer_mmap_space *) malloc(sizeof(struct buffer_mmap_space)); */
    printf("buffer_map_space: %ld\n", sizeof(struct buffer_mmap_space));
    /* struct buffer_mmap_space * mmap_space = (struct buffer_mmap_space * ) mmap(NULL, sizeof(struct buffer_mmap_space), */ 
    /*                                                                            PROT_READ | PROT_WRITE, MAP_SHARED, channel_fd, 0); */
    struct channel_buffer * channel_buffer = (struct channel_buffer *)mmap(NULL, sizeof(struct channel_buffer) * buffer_count,
                                                            PROT_READ | PROT_WRITE, MAP_SHARED, channel_fd, 0);
    /* if(mmap_space == MAP_FAILED) */
    /* { */
    /*     return NULL; */
    /* } */
#ifdef BUFFER_CONTROL_FIELD
    mmap_space->buffer_count = buffer_count;
#endif
    struct channel_buffer_context * context = (struct channel_buffer_context *) malloc(sizeof(struct channel_buffer_context));
    context->channel_buffer = channel_buffer; // mmap_space->channel_buffers;
    return context;
}


void buffer_release(struct channel_buffer_context * buffer_context)
{
    int ret = munmap(buffer_context->channel_buffer, sizeof(struct channel_buffer));
    if(ret == -1)
    {
        printf("Failed to unmap buffer\n");
    }
    free(buffer_context);
}

void buffer_configure(struct channel_buffer_context * buffer_context, buffer_on_fill * on_fill)
{
    buffer_context->on_fill = on_fill;
}
