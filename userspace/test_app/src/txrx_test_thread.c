#include <bits/types/struct_timeval.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <bbio_h.h>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

struct thread_config {
    io_stream_device * dev;
    uint32_t packet_size;
    uint32_t total_size;
    int signal;
};

int stop_tx = 0;
int stop_rx = 0;
int verify = 0;

static void * tx_thread(void * ptr){

    int i, j;
    struct thread_config * config = (struct thread_config *) ptr;
    struct channel_buffer *buffer_test;
    
    uint32_t packet_size = config->packet_size;
    uint32_t packet_loops = config->total_size / config->packet_size;
    io_stream_device *dma_tx = config->dev;

    uint32_t start = 0; // (uint16_t)time(NULL);
    
    printf("TX_THREAD: start=%X packet_size=%d, packet_loops=%d\n", start, packet_size, packet_loops);
    for (int j = 0; j < packet_loops; j++)
    {
        if(stop_tx) break;
        buffer_test = io_stream_get_buffer(dma_tx);
        // memset(buffer_test, 0xff, BUFFER_IN_BYTES(buffer_test));
        
        buffer_test->header.decode_time_stamp = start;
        buffer_test->header.tx_time_stamp = j;
        /* buffer_test->packet_length = packet_size; */ 
        
        BUFFER_LEN_SET(buffer_test, packet_size);

        
        for (i = 0; i < packet_size; i++)
        {
            /* buffer_test->buffer[i].Q0 = start; */
            /* buffer_test->buffer[i].I0 = start + i + j * packet_size; */
            buffer_test->buffer[i] = (start + i + j * packet_size + 1) * 1000;
        }
        

        io_write_stream_device(dma_tx, buffer_test, BUFFER_IN_BYTES(buffer_test));
    }


    // io_sync_stream_device(dma_tx);
    return NULL;
}

static void * rx_thread(void * ptr){

    uint32_t i, j;
    int ret;
    fd_set readfds;
    struct thread_config * config = (struct thread_config *) ptr;
    struct channel_buffer *buffer_test_rx;
    struct timeval timeout =  {
        .tv_sec = 3
    };

    uint32_t packet_size = config->packet_size;
    uint32_t packet_loops = config->total_size / config->packet_size;
    io_stream_device *dma_rx = config->dev;

    printf("RX_THREAD: packet_size=%d packet_loops=%d\n", packet_size, packet_loops);

    i = 0;

    FD_ZERO(&readfds);
    FD_SET(dma_rx->fd, &readfds);
    for (int j = 0; ; j++)
    {

        ret = select(dma_rx->fd + 1, &readfds, NULL, NULL, &timeout);
        if(ret < 0) {
            printf("Poll ret=%d\n", ret);
            continue;
        }else if(ret == 0) {
            printf("Timeout. \n");
            break;
        }

        if(!FD_ISSET(dma_rx->fd, &readfds)) {
            printf("Poll rx not ready");
            continue;

        }


        buffer_test_rx = io_stream_get_buffer(dma_rx);

        struct buffer_header * rx_header = &buffer_test_rx->header;

        rx_header->decode_time_stamp = -1;
        rx_header->tx_time_stamp = -1;
        
        io_read_stream_device(dma_rx, buffer_test_rx, 128 * 1024);

        io_sync_stream_device(dma_rx);

        fprintf(stderr, "\r%d dts=%d tts=%d len=%d start_word=%X ", 
                j + 1, 
                rx_header->decode_time_stamp,
                rx_header->tx_time_stamp,
                rx_header->packet_length,
                buffer_test_rx->buffer[0]);

        if(verify) {
            for(int k = 0; k < rx_header->packet_length; k++ ) {
                if((k + i + 1) != buffer_test_rx->buffer[k]) {
                    printf("X: %d %d\n", k + i + 1, buffer_test_rx->buffer[k]);
                    break;
                }
            }
        }
        i += rx_header->packet_length;
    }
    return NULL;
}

void handle_sigint_exit_tx(int sig) {
    printf("Caught signal %d\n", sig);
    stop_tx = 1;
}
void handle_sigint_exit_rx(int sig) {
    printf("Caught signal %d\n", sig);
    stop_rx = 1;
}

int txrx_test_thread(int argc, char **argv)
{
    pthread_t thread_rx, thread_tx;
    io_context *ctx;
    io_mapped_device *map_dev;
    io_stream_device *dma_tx;
    io_stream_device *dma_rx;
    struct thread_config rx_thread_config, tx_thread_config;

    uint32_t packet_loops = 10000;

    uint32_t packet_size = 1024 * 16;

    uint32_t repack_size = 1024 * 4;
    uint16_t start = 0;
    uint32_t total_size = packet_loops * packet_size;

    signal(SIGINT, handle_sigint_exit_tx);

    start = (uint16_t)time(NULL);

    ctx = io_create_context(); // Create streamio context;

    map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
    dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_1");
    dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");

    io_write_mapped_device(map_dev, 5, 0b11);
    io_write_mapped_device(map_dev, 6, repack_size);
    io_write_mapped_device(map_dev, 7, 0b00010000); // bb data sel reg

    io_write_mapped_device(map_dev, 5, 0b00);
    
    sleep(1);

    rx_thread_config.dev = dma_rx;
    rx_thread_config.packet_size = repack_size;
    rx_thread_config.total_size = total_size;
    rx_thread_config.signal = 0;

    tx_thread_config.dev = dma_tx;
    tx_thread_config.packet_size = packet_size;
    tx_thread_config.total_size = total_size;
    tx_thread_config.signal = 0;

    printf("Creating thread...\n");
    int ret1 = pthread_create(&thread_tx, NULL, tx_thread, &tx_thread_config);
    int ret2 = pthread_create(&thread_rx, NULL, rx_thread, &rx_thread_config);

    pthread_join(thread_tx, NULL);
    printf("TX Thread exited...\n");
    signal(SIGINT, handle_sigint_exit_rx);

    pthread_join(thread_rx, NULL);

    printf("Exiting thread....\n");
    io_write_mapped_device(map_dev, 5, 0b11);
    sleep(0);
    io_close_context(ctx);
exit:   
    return 0;
}
