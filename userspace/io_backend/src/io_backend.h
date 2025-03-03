#include <bbio_backend_net.h>

#define MAX_CONTEXT_COUNT 5

typedef int(*frame_handler)(int client_fd, frame_top * frame);


#define HANDLE_RET_CLOSE        1
#define HANDLE_RET_MAINTAIN     2
#define HANDLE_RET_

