#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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
#include <poll.h>
#include <bbio_h.h>
#include <sys/param.h>

#define TEST_SIZE 512

static int stop = 0;
int main_tx(int argc, char **argv)
{
    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");
    int i, ret;
    io_context *ctx = io_create_context();
    // io_context *ctx = io_create_net_context("192.168.2.5", 12345);
    printf("Add devices\n");

    io_stream_device *dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");

    int test_times = 100, j = 0;

    int test_size = MAX_SAMPLES;
    for (int j = 0; j < test_times; j++)
    {
        struct channel_buffer *buffer_test = io_stream_get_buffer(dma_tx);

        for (i = 0; i < test_size; i++)
        {
            buffer_test->buffer[i].I0 = i + j * test_size;
            buffer_test->buffer[i].Q0 = i + j * test_size;
        }
        io_write_stream_device(dma_tx, buffer_test, test_size * sizeof(iq_buffer));
    }

    io_close_context(ctx);

    return 0;
}


int validate_data(struct channel_buffer *buffer_rx, int test_size, int k, int j)
{
    int i;
    int check_value;
    int err = 0;
    char print_buf[256];
    int offset = 0;

    for (i = 0; i < test_size; i++)
    {
        int value = *((int *)&buffer_rx->buffer[i]);
        check_value = i << 1;
        if (value == check_value)
            continue;
        err = 1;
        printf("Data validate error at %d %d- [%d]:  Expect %d, recieve %d \n", j, k, i + 1, check_value, value);
        for (int pos = (i > 10 ? i - 10 : 0);
             pos < MIN(i + 10, test_size);
             pos++)
        {
            sprintf(print_buf, "%d ", *((int *)&buffer_rx->buffer[pos]));
            printf("%s", print_buf);
            if(pos < i) offset += strlen(print_buf);
        }
        printf("\n");

        printf("%*s", offset, "");
        printf("^ != %d\n", check_value);
        break;
    }

    return -err;
}

void sigint(int a)
{
    printf("Stop transmit...\n");
    stop = 1;
}

int main_rx(int argc, char **argv)
{
    int i, ret, err = 0;
    int test_buffers = 32, j = 0;
    int test_size = 32 * 1024, loop_times = 10000, valid_interval = 100;
    int valid_ok_count = 0;

    io_context *ctx = io_create_context(); //_net_context("192.168.2.5", 12345);

    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");

    printf("Add devices\n");

    io_mapped_device *map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");

    // perform reset
    io_write_mapped_device(map_dev, 5, 3);

    // configure stream size
    io_write_mapped_device(map_dev, 6, test_size);

    io_stream_device *dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    int rx_test_size = test_size; // 256 + 128;

    // release reset
    io_write_mapped_device(map_dev, 5, 0);

    signal(SIGINT, sigint);

    struct channel_buffer *buffer_rx_test; //  = io_stream_get_buffer(dma_rx);

    for (i = 0; i < 32; i++)
    {
        buffer_rx_test = io_stream_get_buffer(dma_rx);
        // printf("buffer addr = %p\n", buffer_rx_test);
        memset(buffer_rx_test, -1, sizeof(iq_buffer) * rx_test_size);
        io_read_stream_device(dma_rx, (void *)buffer_rx_test->buffer, rx_test_size * sizeof(iq_buffer));
    }

    while (loop_times && !stop)
    {
        for (j = 0; (j < test_buffers) && !stop; j++)
        {
            buffer_rx_test = io_stream_get_buffer(dma_rx);

            // io_sync_stream_device(dma_rx);
            if(validate_data(buffer_rx_test, test_size, j, 0) < 0) {
                goto exit;
            }else {
                if(((valid_ok_count % valid_interval) + 1) == valid_interval) {
                    //  fputc('-', stderr);
                }
                valid_ok_count++;
            }

            if (!buffer_rx_test)
            {
                printf("Buffer request timeout\n");
                goto force_exit;
            }
            memset(buffer_rx_test->buffer, -1, sizeof(iq_buffer) * rx_test_size);
            io_read_stream_device(dma_rx, buffer_rx_test, rx_test_size * sizeof(iq_buffer));
            // buffer_rx_test = io_stream_get_buffer(dma_rx);
            // io_finish_stream_local(dma_rx);
        }

        loop_times--;
    }
    printf("\n");
exit:
    io_sync_stream_device(dma_rx);
force_exit:
    io_write_mapped_device(map_dev, 5, 3);

    io_close_context(ctx);

    return 0;
}

int main(int argc, char **argv)
{
    return main_rx(argc, argv);
}