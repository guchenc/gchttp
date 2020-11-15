#include "acceptor.h"

struct acceptor* acceptor_new(int port)
{
    struct acceptor* acceptor = malloc(struct acceptor);
    if (acceptor == NULL) goto failed; 

    int ret = 0;
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        LOG(LT_ERROR, "%s", strerrno(errno));
        goto failed;
    }

    // 设置地址复用，这样即便服务器主动关闭，处于TIME_WAIT状态，仍然可以重新使用端口号，快速重启
    int on = 1;
    ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerrno(errno));
        goto failed;
    }

    // 设置非阻塞监听套接字
    ret = make_nonblocking(listenfd);
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerrno(errno));
        goto failed;
    }

    ret = bind(listenfd, (SA*)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerrno(errno));
        goto failed;
    }

    ret = listen(listenfd, LISTENQ);
    if (ret < 0) {
        LOG(LT_ERROR, "%s", strerrno(errno));
        goto failed;
    }

    acceptor->listen_fd = listenfd;
    acceptor->listen_port = port;
    acceptor->connAccepted = 0;

    return acceptor;

failed:
    LOG(LT_FATAL_ERROR, "failed to create acceptor on port %d!", port);
    if (acceptor != NULL) free(acceptor);
    if (listenfd > 0) close(listenfd);
    return NULL;
}
