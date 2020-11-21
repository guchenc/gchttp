#include "acceptor.h"

struct acceptor* acceptor_new(int type, int port)
{
    struct acceptor* acceptor = malloc(sizeof(struct acceptor));
    if (acceptor == NULL) goto failed; 

    int ret = 0;

    // TODO:only using ipv4 server address for now, supporting ipv6 server address later
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int listenfd = -1;
    if (type == TCP_SERVER) {
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        LOG(LT_DEBUG, "using tcp acceptor, listen fd = %d", listenfd);
    } else if (type == UDP_SERVER) {
        listenfd = socket(AF_INET, SOCK_DGRAM, 0);
        LOG(LT_DEBUG, "using udp acceptor, listen fd = %d", listenfd);
    } else {
        LOG(LT_FATAL_ERROR, "unknown server type %d", type);
        goto failed;
    }
    if (listenfd < 0) {
        LOG(LT_ERROR, "%s", strerror(errno));
        goto failed;
    }

    // 设置非阻塞监听套接字
    make_nonblocking(listenfd);

    // 设置地址复用，这样即便服务器主动关闭，处于TIME_WAIT状态，仍然可以重新使用端口号，快速重启
    int on = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerror(errno));
        goto failed;
    }

    ret = bind(listenfd, (SA*)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerror(errno));
        goto failed;
    }

    ret = listen(listenfd, LISTENQ);
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerror(errno));
        goto failed;
    }

    acceptor->type = type;
    acceptor->listen_fd = listenfd;
    acceptor->listen_port = port;
    acceptor->connAccepted = 0;

    return acceptor;

failed:
    if (acceptor != NULL) free(acceptor);
    if (listenfd > 0) close(listenfd);
    LOG(LT_FATAL_ERROR, "failed to create acceptor on port %d!", port);
    return NULL;
}

void acceptor_cleanup(struct acceptor* acceptor)
{
    if (acceptor == NULL) return;
    if (acceptor->listen_fd > 0) close(acceptor->listen_fd);
}
