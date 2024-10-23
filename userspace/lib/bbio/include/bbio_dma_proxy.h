#include <sys/ioctl.h>
#include <stdint.h>

#define TX_BUFFER_COUNT 32 /* app only, must be <= to the number in the driver */
#define RX_BUFFER_COUNT 32 /* app only, must be <= to the number in the driver */
#define BUFFER_INCREMENT 1 /* normally 1, but skipping buffers (2) defeats prefetching in the CPU */

#define FINISH_XFER _IOW('a', 'a', int32_t *)
#define START_XFER _IOW('a', 'b', int32_t *)
#define XFER _IOR('a', 'c', int32_t *)
#define SELECT 			_IOW('a','d',int32_t*)


