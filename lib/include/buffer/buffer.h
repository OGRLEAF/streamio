#include <stdint.h>
#include "bbio_dma_proxy.h"

#ifndef _H_BUFFER
#define _H_BUFFER

#define BUFFER_SIZE (128 * 1024)	 /* must match driver exactly */
#define BUFFER_COUNT TX_BUFFER_COUNT /* driver only */
// #define BUFFER_INCREMENT 1
#define _MAX_SAMPLES(SAMPLE_TYPE) (BUFFER_SIZE / sizeof(SAMPLE_TYPE))

typedef struct _iq_buffer
{
	int16_t I0;
	int16_t Q0;
} iq_buffer;

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
	iq_buffer buffer[_MAX_SAMPLES(iq_buffer)];
	enum proxy_status status;
	unsigned int length;
} __attribute__((aligned(1024)));

typedef uint32_t buffer_on_fill(struct channel_buffer *buffer, iq_buffer *buffer_start, iq_buffer *buffer_end, int sample_n, void *private);

struct channel_buffer_context
{
	struct channel_buffer *channel_buffer;
	buffer_on_fill *on_fill;
	void *private;
};

struct channel_buffer_context *buffer_allocate(int buffer_count, int channel_fd);
void buffer_release(struct channel_buffer_context *buffer_context);

#endif