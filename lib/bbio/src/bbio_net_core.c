#include <string.h>

#include "bbio_h.h"
#include "bbio_private_h.h"
#include "bbio_backend_net.h"

io_context *io_create_net_context(char *host, uint16_t port)
{
    struct hostent *server;
    int ret;
    char addr_str[INET_ADDRSTRLEN];

    io_net_context *ctx_net = (io_net_context *)malloc(sizeof(io_net_context));

    server = gethostbyname(host);
    if (server == NULL)
    {
        printf("No such host: %s\n", host);
        return NULL;
    }

    inet_ntop(AF_INET, server->h_addr_list[0], addr_str, INET_ADDRSTRLEN);
    printf("Server %s resolved to %s\n", host, addr_str);


    memset((char *)&ctx_net->serv_addr, 0, sizeof(ctx_net->serv_addr));
    ctx_net->serv_addr.sin_family = AF_INET;

    memcpy((char *)&ctx_net->serv_addr.sin_addr.s_addr, (char *)server->h_addr_list[0], server->h_length);
    ctx_net->serv_addr.sin_port = htons(port);

    
    io_context *ctx = (io_context *)ctx_net;

    ctx->devices = (io_device **)malloc(1 * sizeof(io_device *));
    ctx->devices[0] = NULL;
    ctx->backend.open_mapped = io_open_mapped_net;
    ctx->backend.open_stream = io_open_stream_net;
    ctx->backend.close_mapped = io_close_mapped_net;
    ctx->backend.close_stream = io_close_stream_net;

    if (pthread_mutex_init(&ctx_net->lock, NULL) != 0)
    {
        printf("Warn: mutex init has failed\n");
    }
    return ctx;
}


