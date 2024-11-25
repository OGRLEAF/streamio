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
#include <time.h>
#include "tc_test.h"

#define TEST_SIZE 512

static int stop = 0;
extern struct argp argp;

int main_tx_test(int argc, char **argv)
{
    int i, ret;

    io_context *ctx;
    io_mapped_device *map_dev;
    io_stream_device *dma_tx;
    io_stream_device *dma_rx;
    struct channel_buffer *buffer_test;
    struct channel_buffer *buffer_test_rx;
    int packet_loops = 1, j = 0;

    int packet_size = 640;
    uint16_t start = (uint16_t)time(NULL);

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");
    dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    io_write_mapped_device(map_dev, 5, 0b11);
    io_write_mapped_device(map_dev, 6, packet_size);
    io_write_mapped_device(map_dev, 7, 0b00010000); // bb data sel reg

    io_write_mapped_device(map_dev, 5, 0b00);
    printf("Test data start from 0x%X\n", start);

    for (int j = 0; j < packet_loops; j++)
    {
        buffer_test = io_stream_get_buffer(dma_tx);
        buffer_test_rx = io_stream_get_buffer(dma_rx);

        for (i = 0; i < packet_size; i++)
        {
            buffer_test->buffer[i].Q0 = start;
            buffer_test->buffer[i].I0 = start + i + j * packet_size;
        }

        io_write_stream_device(dma_tx, buffer_test, packet_size * sizeof(iq_buffer));
        io_read_stream_device(dma_rx, buffer_test_rx, packet_size * sizeof(iq_buffer));
    }
    io_sync_stream_device(dma_tx);
    io_sync_stream_device(dma_rx);

    printf("%X - %X\n", (uint16_t)buffer_test_rx->buffer[packet_size - 1].I0, (uint16_t)buffer_test_rx->buffer[packet_size - 1].Q0);

    printf("Test data end at 0x%X - 0x%X\n", (uint16_t)(start + packet_loops * packet_size - 1), (uint16_t) ~(start + packet_loops * packet_size - 1));

    io_write_mapped_device(map_dev, 5, 0b11);
    io_close_context(ctx);
exit:
    return 0;
}

int main_tx_file(int argc, char **argv)
{
    struct arguments arguments;
    int i, ret;
    io_context *ctx;

    io_mapped_device *map_dev;
    io_stream_device *dma_tx;
    io_stream_device *dma_rx;
    struct channel_buffer *buffer_test;
    struct channel_buffer *buffer_test_rx;

    int data_size = 0;
    int packet_loops = 1;
    int packet_size = 20480;
    
    uint16_t start = (uint16_t)time(NULL);

    FILE *input_f, *output_f;
    char line[256];
    uint32_t *input_temp, *output_temp;

    arguments.input_file_path = NULL;
    arguments.output_file_path = NULL;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (arguments.input_file_path)
    {
        printf("Open file %s\n", arguments.input_file_path);
        input_f = fopen(arguments.input_file_path, "r");
        if (input_f == NULL)
        {
            fprintf(stderr, "Open input file %s failed %d\n", arguments.input_file_path, input_f);
            return -1;
        }
        // get data size by go through all lines;
        
        while (fgets(line, sizeof(line), input_f))
        {

            uint32_t value;
            if (sscanf(line, "%u", &value) == 1)
            {
                data_size += 1;
            }
            else
            {
                printf("Invalid line %d: %s cannot parse to integer\n", data_size, line);
                fclose(input_f);
                return -1;
            }
        }
        fseek(input_f, 0, SEEK_SET);
        // cacluate loops
        if (data_size <= packet_size)
        {
            packet_loops = 1;
            packet_size = data_size;
        }
        else
        {
            packet_loops = data_size % packet_size;
        }
        // Read data to buffer
        input_temp = (uint32_t *)malloc(sizeof(uint32_t) * data_size);

        while (fgets(line, sizeof(line), input_f))
        {
            if (sscanf(line, "%u", input_temp) == 1)
            {
                input_temp++;
            }
        }
        input_temp -= data_size;
    }

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");
    dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    io_write_mapped_device(map_dev, 5, 0b11);
    io_write_mapped_device(map_dev, 6, packet_size);
    io_write_mapped_device(map_dev, 7, 0b00010000); // bb data sel reg

    io_write_mapped_device(map_dev, 5, 0b00);
    printf("Test data start from 0x%X packet_loops=%d packet_size=%d\n", *input_temp, packet_loops, packet_size);

    output_temp = (uint32_t *)malloc(sizeof(uint32_t) * packet_loops * packet_size);

    for (int j = 0; j < packet_loops; j++)
    {
        buffer_test = io_stream_get_buffer(dma_tx);

        memcpy((uint32_t *)buffer_test->buffer, (uint32_t *)(input_temp + j * packet_size), packet_size * sizeof(iq_buffer));
        io_write_stream_device(dma_tx, buffer_test, packet_size * sizeof(iq_buffer));

        buffer_test_rx = io_stream_get_buffer(dma_rx);
        io_read_stream_device(dma_rx, buffer_test_rx, packet_size * sizeof(iq_buffer));

        // Copy last recieve buffer
        io_sync_stream_device(dma_rx);
        memcpy((uint32_t *)(output_temp + j * packet_size), (uint32_t *)(buffer_test_rx->buffer), packet_size * sizeof(iq_buffer));
    }
    io_sync_stream_device(dma_tx);
    io_sync_stream_device(dma_rx);

    printf("last output data 0x%.8X\n", (uint32_t)output_temp[packet_size * packet_loops - 1]);

    printf("test data expect 0x%.4X%.4X\n", (uint16_t)~input_temp[packet_size * packet_loops - 1],
                                            (uint16_t)input_temp[packet_size * packet_loops - 1]);

    io_write_mapped_device(map_dev, 5, 0b11);
    io_close_context(ctx);

    if (arguments.output_file_path)
    {
        output_f = fopen(arguments.output_file_path, "w");
        if (output_f == NULL)
        {
            fprintf(stderr, "Open output file %s failed %d\n", arguments.output_file_path, output_f);
            return -1;
        }
        for (i = 0; i < data_size; i++)
        {
            fprintf(output_f, "0X%X\n", output_temp[i]);
        }
        fclose(output_f);
    }

exit:
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
            if (pos < i)
                offset += strlen(print_buf);
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
    io_write_mapped_device(map_dev, 5, 0b11);

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
            if (validate_data(buffer_rx_test, test_size, j, 0) < 0)
            {
                goto exit;
            }
            else
            {
                if (((valid_ok_count % valid_interval) + 1) == valid_interval)
                {
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
    return main_tx_file(argc, argv);
}