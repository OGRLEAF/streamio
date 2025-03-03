#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#include <bbio_h.h>
#include <sys/param.h>

#define TEST_SIZE 512

int main_tx(io_context *ctx)
{
    int i, ret;
    io_stream_device *dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");

    int test_times = 10000, j = 0;

    int test_size = MAX_SAMPLES;
    for (int j = 0; j < test_times; j++)
    {
        struct channel_buffer *buffer_test = io_stream_get_buffer(dma_tx);

        for (i = 0; i < test_size; i++)
        {
            // buffer_test->buffer[i].I0 = i + j * test_size;
            // buffer_test->buffer[i].Q0 = i + j * test_size;
        }
        io_write_stream_device(dma_tx, buffer_test, test_size * sizeof(iq_buffer));
    }

    io_close_context(ctx);

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

int main_rx(io_context *ctx)
{
    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");
    int i = 0, ret;

    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");

    printf("Add devices\n");

    io_mapped_device *map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    int test_buffers = 1, j = 0;
    
    struct channel_buffer *buffers_rx_test[32];
    
    int test_size = 1024 * 2, loop_times = 10000000;

    int validate = 1000, valid_ok_count = 0;

    // perform reset1
    io_write_mapped_device(map_dev, 5, 3);

    // configure stream size
    io_write_mapped_device(map_dev, 6, test_size);

    // release reset
    io_write_mapped_device(map_dev, 5, 0);

    io_stream_device *dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");
    io_stream_device *dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");

    int rx_test_size = test_size;

    signal(SIGINT, sigint);

    // goto exit;
    while ((i < loop_times) && !stop)
    {
        for (j = 0; (j < test_buffers) && !stop; j++)
        {
            struct channel_buffer *buffer_tx_test = io_stream_get_buffer(dma_tx); 
            struct channel_buffer *buffer_rx_test = io_stream_get_buffer(dma_rx);
            
            for(int k=0; k < test_size; k++ ) {
                // buffer_rx_test->buffer[k].I0 = k;
                // buffer_tx_test->buffer[k].Q0 = k;
            }

            // io_write_stream_device(dma_tx, buffer_tx_test, sizeof(iq_buffer) * test_size);


            if (!buffer_rx_test)
            {
                printf("Buffer request timeout\n");
                goto exit;
            }
            memset(buffer_rx_test->buffer, -2, sizeof(iq_buffer) * rx_test_size);
            io_read_stream_device(dma_rx, buffer_rx_test, sizeof(iq_buffer) * test_size);

            if (validate_data(buffer_rx_test, test_size, i, j) < 0)
            {
                goto force_exit;
            }
            else
            {
                valid_ok_count = (valid_ok_count + 1) % validate;
                if (valid_ok_count == validate - 1){
                    fputc('-', stderr);
                }
            }
        }
        io_sync_stream_device(dma_rx);

        i++;
    }

exit:
    putc('\n', stderr);
    // io_sync_stream_device(dma_rx);
force_exit:

    io_close_context(ctx);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: io_client <ip> <port>\n");
        return 0;
    }
    io_context *ctx = io_create_net_context(argv[1], atoi(argv[2]));
    if (!ctx)
    {
        return -1;
    }

    // return main_rx(ctx);
    return 0;
}
