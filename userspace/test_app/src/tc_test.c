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
#include <poll.h>
#include <bbio_h.h>
#include <sys/param.h>
#include <time.h>
#include <math.h>
#include "tc_test.h"

#define TEST_SIZE 512
#define BUFFER_IQ_BUFFER

static int stop = 0;
extern struct argp argp;

static int main_txrx_test(int argc, char **argv)
{
    int i, ret;

    io_context *ctx;
    io_mapped_device *map_dev;
    io_stream_device *dma_tx;
    io_stream_device *dma_rx;
    struct channel_buffer *buffer_test;
    struct channel_buffer *buffer_test_rx;
    int packet_loops = 1000, j = 0;

    int packet_size = 1024 * 16;
    int repack_size = 1024 * 16;
    uint16_t start = 0;

    start = (uint16_t)time(NULL);

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_1");
    dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    io_write_mapped_device(map_dev, 5, 0b11);
    io_write_mapped_device(map_dev, 6, repack_size);
    io_write_mapped_device(map_dev, 7, 0b00010000); // bb data sel reg

    io_write_mapped_device(map_dev, 5, 0b00);

    printf("data start from %X\n", start );
    for (int j = 0; j < packet_loops; j++)
    {
        buffer_test = io_stream_get_buffer(dma_tx);
        memset(buffer_test, 0xff, BUFFER_IN_BYTES(buffer_test));
        
        buffer_test->header.decode_time_stamp = start;
        buffer_test->header.tx_time_stamp = j;
        /* buffer_test->packet_length = packet_size; */ 
        
        BUFFER_LEN_SET(buffer_test, packet_size);


        buffer_test_rx = io_stream_get_buffer(dma_rx);
        struct buffer_header * rx_header = &buffer_test_rx->header;
        rx_header->decode_time_stamp = -1;
        rx_header->tx_time_stamp = -1;
        
        for (i = 0; i < packet_size; i++)
        {
            /* buffer_test->buffer[i].Q0 = start; */
            /* buffer_test->buffer[i].I0 = start + i + j * packet_size; */
            buffer_test->buffer[i] = start + i + j * packet_size;
        }
        

        io_write_stream_device(dma_tx, buffer_test, BUFFER_IN_BYTES(buffer_test));
        
        io_read_stream_device(dma_rx, buffer_test_rx, 128 * 1024);

        io_sync_stream_device(dma_tx);

        io_sync_stream_device(dma_rx);

        printf("buffer header\n\tdts = %d\n\ttts = %d\n\tpacket_size = %d\n",
            rx_header->decode_time_stamp, rx_header->tx_time_stamp, rx_header->packet_length);
        /* printf("%X - %X\n", (uint16_t)buffer_test_rx->buffer[0], */ 
        /*                             (uint16_t)(buffer_test_rx->buffer[buffer_test_rx->packet_length - 1])); */
    }
    
   
    
    printf("Test data end at 0x%X - 0x%X\n", (uint16_t)(start + packet_loops * packet_size - 1), (uint16_t) ~(start + packet_loops * packet_size - 1));

    io_write_mapped_device(map_dev, 5, 0b11);
    io_close_context(ctx);
exit:   
    return 0;
}

int main_tx_test(int argc, char **argv)
{
    int i, ret;

    io_context *ctx;
    io_mapped_device *map_dev;
    io_stream_device *dma_tx;
    struct channel_buffer *buffer_test;
    struct buffer_header * buffer_test_header;
    int packet_loops = 2000000, j = 0;

    int packet_size = 1024 * 2;
    int repack_size = 1024;
    uint16_t start = 0;
    uint32_t points = 128;

    start = (uint16_t)time(NULL);

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/axi_dma_1");

    io_write_mapped_device(map_dev, 5, 0b11);
    io_write_mapped_device(map_dev, 4, 0b1);
    io_write_mapped_device(map_dev, 6, repack_size);
    io_write_mapped_device(map_dev, 7, 0b00000000); // bb data sel reg

    io_write_mapped_device(map_dev, 5, 0b00);

  
    double delta =  (M_PI * 2) / points;
    uint32_t phase = 1;

    iq_buffer *data_copy = (iq_buffer *) malloc(sizeof(iq_buffer) * points);

    double scale = 65535.0 / 2;
    for(i = 0;i < points;i++) {
        data_copy[i].I0 = (short) (sin(i * delta ) * scale);
        data_copy[i].Q0 = (short) (cos(i * delta ) * scale);
    }


    for (int j = 0; j < packet_loops; j++)
    {
        buffer_test = io_stream_get_buffer(dma_tx);
        
        buffer_test_header = &buffer_test->header;

        buffer_test_header->decode_time_stamp = j;
        buffer_test_header->tx_time_stamp = j + 1; // 0x0f0f0f0f;
        
        BUFFER_LEN_SET(buffer_test, packet_size);

        for (i = 0; i < (packet_size); i++)
        {
            /* buffer_test->buffer[i].Q0 = start; */
            /* buffer_test->buffer[i].I0 = start+ i + j * packet_size; */

            /* value_q = cos(phase) * (10000.0); */
            /* *(buffer_test->buffer + i) = *((uint32_t*) (data_copy + (phase & 0b1111111))); */
            buffer_test->buffer[i] = phase & 0xffff; 
            phase += 1 << 8;
        }
        

        io_write_stream_device(dma_tx, buffer_test, BUFFER_IN_BYTES(buffer_test));
        
        /* io_sync_stream_device(dma_tx); */

    }   
    
    io_sync_stream_device(dma_tx);

    io_write_mapped_device(map_dev, 5, 0b11);
    io_close_context(ctx);
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
    int packet_size = 1024 * 32;

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
            fprintf(stderr, "Open input file %s failed %p\n", arguments.input_file_path, input_f);
            return -1;
        }

        while (fgets(line, sizeof(line), input_f))
        {

            uint32_t value;
            if (sscanf(line, "%u", &value) == 1)
            {
                data_size += 1;
            }
            else
            {
                printf("Invalid line %d: %s cannot parse to integer\n", data_size + 1, line);
                fclose(input_f);
                return -1;
            }
        }
        fseek(input_f, 0, SEEK_SET);

        // Read data to buffer
        input_temp = (uint32_t *)malloc(sizeof(uint32_t) * data_size);

        while (fgets(line, sizeof(line), input_f))
            input_temp += sscanf(line, "%u", input_temp) == 1;
        input_temp -= data_size;

        fclose(input_f);
    }
    else
    {
        return -1;
    }

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");
    dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    // Write AXI/AXI-Lite module registers examples
    // reg5: [0:0] maxis_reset
    //       [1:1] external fifo reset
    // reg6: maxis packet size
    // reg7: data selection
    //       [3:0]  maxis input data selection: avaliable: 0: in_data0; 1: in_data1
    //       [7:4]  baseband data selection: 0: DDS (input stream as phase word); 1: ~input
    io_write_mapped_device(map_dev, 5, 0b11);       // hold reset signals
    io_write_mapped_device(map_dev, 6, packet_size);
    io_write_mapped_device(map_dev, 7, 0b00000000); // bb data sel reg
    io_write_mapped_device(map_dev, 5, 0b00);       // release reset signals

    printf("Test data start from 0x%.4x packet_size=%d(%.2fKbytes) totalsize=%d(%.2fKbytes)\n",
           *input_temp,
           packet_size, packet_size * sizeof(iq_buffer) / 1024.0, data_size, data_size * sizeof(iq_buffer) / 1024.0);

    output_temp = (uint32_t *)malloc(sizeof(uint32_t) * data_size);

    int j = 0;
    while (data_size > 0)
    {
        int tx_size = MIN(packet_size, data_size);

        buffer_test = io_stream_get_buffer(dma_tx);

        memcpy((uint32_t *)buffer_test->buffer, (uint32_t *)(input_temp + j), tx_size * sizeof(iq_buffer));
        io_write_stream_device(dma_tx, buffer_test, tx_size * sizeof(iq_buffer));

        buffer_test_rx = io_stream_get_buffer(dma_rx);
        io_read_stream_device(dma_rx, buffer_test_rx, packet_size * sizeof(iq_buffer));

        io_sync_stream_device(dma_rx);
        memcpy((uint32_t *)(output_temp + j), (uint32_t *)(buffer_test_rx->buffer), tx_size * sizeof(iq_buffer));

        data_size -= tx_size;
        j += tx_size;
    }
    io_sync_stream_device(dma_tx);
    io_sync_stream_device(dma_rx);

    printf("last output data 0x%.8x, expect 0x%.4x%.4x\n", (uint32_t)output_temp[j - 1],
           (uint16_t)~input_temp[j - 1], (uint16_t)input_temp[j - 1]);

    io_write_mapped_device(map_dev, 5, 0b11);
    io_close_context(ctx);

    if (arguments.output_file_path)
    {
        output_f = fopen(arguments.output_file_path, "w");
        if (output_f == NULL)
        {
            fprintf(stderr, "Open output file %s failed %p\n", arguments.output_file_path, output_f);
            return -1;
        }
        for (i = 0; i < j; i++)
        {
            fprintf(output_f, "%d\n", output_temp[i]);
        }
        fclose(output_f);
    }

exit:
    return 0;
}

int main(int argc, char **argv)
{
    // return txrx_test_thread(argc, argv);  // TX &  RX data multithreading
    // return main_txrx_test(argc, argv);    // TX & RX in same thread
    return main_tx_test(argc, argv);      // RX only (For RF project)
    // return main_tx_file(argc, argv);      // TX & RX from file to file
}
