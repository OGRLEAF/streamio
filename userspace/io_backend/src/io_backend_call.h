#include <stdint.h>
#include "bbio_h.h"
#include "bbio_backend_net.h"

#define SYS_CALL 0
#define MEM_RW 1


typedef struct _device_open_option {
    char *file_path;
    size_t size;
}  io_open_option;

typedef struct _cmd
{
    uint8_t cmd_type : 1; // SYS_CALL = 0; RW = 1
    uint8_t call_type : 7;
} io_call_cmd;


int client_mapped_cmd_handle(io_mapped_device *device, int sockfd);
int client_stream_cmd_handle(io_stream_device *device, int sockfd);