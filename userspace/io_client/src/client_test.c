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
#include "bbio_h.h"

#define TEST_SIZE 512

int main__(int argc, char **argv)
{
    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");
    int i, ret;
    if (argc < 3)
    {
        return 0;
    }
    io_context *ctx = io_create_net_context(argv[1], atoi(argv[2]));
    if (!ctx)
    {
        return -1;
    }
    printf("Add devices\n");
    io_mapped_device *dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");

    uint32_t test_addr = 5;
    int read_value = io_read_mapped_device(dev, test_addr);
    printf("Read value=%d\n", read_value);

    printf("Try write...\n");
    io_write_mapped_device(dev, test_addr, read_value + 1);

    read_value = io_read_mapped_device(dev, test_addr);
    printf("Read value=%d\n", read_value);

    io_stream_device *dma_dev = (io_stream_device *)io_add_stream_device(ctx, "/dev/dma_proxy_tx");

    // io_write_stream_device(dma_dev, buffer_test, test_size * sizeof(iq_buffer));
    while (1)
    {
        struct channel_buffer *buffer_test = io_stream_get_buffer(dma_dev);

        int test_size = MAX_SAMPLES;
        for (i = 0; i < test_size; i++)
        {
            buffer_test->buffer[i].I0 = i;
            buffer_test->buffer[i].Q0 = i;
        }

        // printf("Sync buffer samples=%d buffer=%p\n", test_size, buffer_test);

        io_write_stream_device(dma_dev, buffer_test, test_size * sizeof(iq_buffer));
    }
    return 0;
}

static int stop = 0;
void sigint(int a)
{
    printf("Stop transmit...\n");
    stop = 1;
}

int validate_data(struct channel_buffer *buffer_rx, int test_size, int k, int j)
{
    int i;
    int check_value;
    int err = 0;
    for (i = 0; i < test_size; i++)
    {
        int value = *((int *)&buffer_rx->buffer[i]);
        check_value = i << 1;
        if (value == check_value)
            continue;
        err = 1;
        printf("Data validate error at %d %d- [%d]:  Expect %d, recieve %d \n", j, k, i + 1, check_value, value);
        break;
    }

    if (!err)
    {
        putc('-', stderr);
        return 0;
    }
    else
    {
        putc('X', stderr);
        return -1;
        // for (i = 0; i < test_size; i++)
        // {
        //     int value = *((int *)&buffer_rx->buffer[i]);
        //     printf("%d ", value);
        // }
        // printf("\n");
    }
}

int main(int argc, char **argv)
{
    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");
    int i, ret;
    if (argc < 3)
    {
        return 0;
    }
    io_context *ctx = io_create_net_context(argv[1], atoi(argv[2]));
    if (!ctx)
    {
        return -1;
    }

    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");

    printf("Add devices\n");

    io_mapped_device *map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    int test_buffers = 32, j = 0;
    struct channel_buffer *buffers_rx_test[32];
    int test_size = 1024, loop_times = 500;

    int validate = loop_times /loop_times;


    // perform reset1
    io_write_mapped_device(map_dev, 5, 3);

    // configure stream size
    io_write_mapped_device(map_dev, 6, test_size);

    // release reset
    io_write_mapped_device(map_dev, 5, 0);

    io_stream_device *dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/dma_mm_rx");

    int rx_test_size = test_size; // 256 + 128;


    signal(SIGINT, sigint);

    // goto exit;

    while (loop_times && !stop)
    {
        for (j = 0; (j < test_buffers) && !stop; j++)
        {
            struct channel_buffer *buffer_rx_test = io_stream_get_buffer(dma_rx);

            if (!buffer_rx_test)
            {
                printf("Buffer request timeout\n");
                goto exit;
            }
            memset(buffer_rx_test->buffer, -1, sizeof(iq_buffer) * rx_test_size);
            io_read_stream_device(dma_rx, buffer_rx_test, sizeof(iq_buffer) * test_size);
            if(((loop_times * test_buffers + j) % validate) == 0) {
                if(validate_data(buffer_rx_test, test_size, loop_times, j) < 0) {
                    goto exit;
                }
            }

        }
        io_sync_stream_device(dma_rx);

        loop_times--;
    }
    putc('\n', stderr);
exit:
    io_sync_stream_device(dma_rx);
force_exit:
    
    io_close_context(ctx);
    
}