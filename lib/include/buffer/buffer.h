#include <stdint.h>
#include "bbio_dma_proxy.h"

#ifndef _H_BUFFER
#define _H_BUFFER

/* #define BUFFER_WITH_CONFIG */

#define BUFFER_SIZE (128 * 1024)	 /* must match driver exactly */
#define BUFFER_COUNT TX_BUFFER_COUNT /* driver only */
// #define BUFFER_INCREMENT 1
#define _MAX_SAMPLES(SAMPLE_TYPE) (BUFFER_SIZE / sizeof(SAMPLE_TYPE))

typedef struct _iq_buffer
{
	int16_t I0;
	int16_t Q0;
} iq_buffer;

typedef uint32_t raw_buffer;

typedef
#ifdef BUFFER_IQ_BUFFER
    iq_buffer;
#else
    raw_buffer 
#endif
buffer_word_t;

#define MAX_SAMPLES _MAX_SAMPLES(iq_buffer)

enum proxy_status
{
	PROXY_NO_ERROR = 0,
	PROXY_BUSY = 1,
	PROXY_TIMEOUT = 2,
	PROXY_ERROR = 3,
	PROXY_QUEUED = 4
};
struct channel_buffer
{
#ifdef BUFFER_WITH_CONFIG
    uint32_t  decode_time_stamp;
    uint32_t  tx_time_stamp;
    uint32_t  packet_length;
	buffer_word_t buffer[_MAX_SAMPLES(buffer_word_t) - 3];
#elif
	buffer_word_t buffer[_MAX_SAMPLES(buffer_word_t)];
#endif
	enum proxy_status status;
	unsigned int length;
} __attribute__((aligned(1024)));

typedef uint32_t buffer_on_fill(struct channel_buffer *buffer, buffer_word_t *buffer_start, buffer_word_t *buffer_end, int sample_n, void *private);

struct buffer_mmap_space {
#ifdef BUFFER_CONTROL_FIELD
    uint32_t buffer_count;
    uint32_t next_buffer_id;
    uint8_t buffer_state[BUFFER_COUNT];
#endif
    struct channel_buffer channel_buffers[BUFFER_COUNT];
};

struct channel_buffer_context
{
	struct channel_buffer *channel_buffer;
	buffer_on_fill *on_fill;
	void *private;
};

#ifdef BUFFER_WITH_CONFIG
#define BUFFER_HEADER_SIZE 3
#else
#define BUFFER_HEADER_SIZE 0
#endif

#define BUFFER_LEN_SET(ch_bufp, len)  (ch_bufp->packet_length = len + BUFFER_HEADER_SIZE)
#define BUFFER_IN_BYTES(ch_bufp) (ch_bufp->packet_length * sizeof(buffer_word_t))

struct channel_buffer_context *buffer_allocate(int buffer_count, int channel_fd);
void buffer_release(struct channel_buffer_context *buffer_context);

#endif
