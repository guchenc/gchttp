#ifndef COMMON_H
#define COMMON_H
#include "log.h"

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h> // timeval for select
#include <sys/types.h>
#include <time.h> // timespec for pselect
#include <unistd.h>
#ifdef EPOLL_ENABLE
#include <sys/epoll.h>
#endif

#define SERVER_PORT 8080
#define LISTENQ 1024
#define UNIXSTR_PATH "/var/lib/unixstream.sock"

#define MAXLINE 4096
#define BUFFER_SIZE 4096

typedef struct sockaddr SA;

void make_nonblocking(int fd); // 将一个套接字设置为非阻塞

#endif
