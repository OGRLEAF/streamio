#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "bbio_h.h"
#include "bbio_private_h.h"
#include "bbio_backend_net.h"

void io_post_context_close_net(io_context *ctx);

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

    // Create context on remote
    frame_top open_ctx_hdr = FRAME_OPEN_CTX;
    frame_ctx_top frame_ctx;
    int sockfd, remotefd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0) {
        printf("Failed to initiate socket\n");
        return NULL;
    }

    ret = connect(sockfd, (struct sockaddr *) &ctx_net->serv_addr, sizeof(ctx_net->serv_addr));

    if (ret < 0) {
        printf("Failed to connect to remote backened.\n") ;
        return NULL;
    }

    ret = send(sockfd, &open_ctx_hdr, sizeof(frame_top), 0);
    if(ret < 0 ) {
        printf("Failed to initiate connection when creating remote context.\n");
        return NULL;
    }

    ret = recv(sockfd, &frame_ctx, sizeof(frame_ctx_top), 0);
    if(ret < 0)
    {
        printf("Failed to get remote context\n");
        return NULL;
    }

    printf("Initiate remote context id=%d\n", frame_ctx.ctx_id);

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


void io_post_context_close_net(io_context *ctx)
{
    int ret;
    io_net_context *ctx_net = (io_net_context *) ctx;
    frame_top close_ctx_hdr = FRAME_CLOSE_CTX;
    
    send(ctx_net->sockfd, )

    close(ctx_net->sockfd);

}
