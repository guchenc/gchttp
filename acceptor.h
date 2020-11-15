#ifndef ACCEPTOR_H
#define ACCEPTOR_H
#include "common.h"

/* 接收器的抽象，对于服务器而言就是监听套接字 */
struct acceptor {
    int listen_port;
    int listen_fd;
    long long connAccepted;
};

/* 初始化一个接收器 */
struct acceptor* acceptor_new(int port);

#endif
