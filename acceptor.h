#ifndef ACCEPTOR_H
#define ACCEPTOR_H
#include "common.h"

/* acceptor abstration, for server is listening socket */
struct acceptor {
    /* server listening port */
    int listen_port;
    /* fd of listening port */
    int listen_fd;
    /* totalnum of connections accepted by acceptor */
    long long connAccepted;
};

/* create and init a tcp/udp acceptor listening on certain port */
struct acceptor* acceptor_new(int type, int port);

/* clean up struct acceptor */
void acceptor_cleanup(struct acceptor* acceptor);

#endif
