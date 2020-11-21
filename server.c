#include "tcp_server.h"

struct server*
server_new(const char* name, int type, int port, int threadNum,
        conn_established_call_back connEstablishedCallBack,
        conn_msg_read_call_back connMsgReadCallBack,
        conn_msg_write_call_back connMsgWriteCallBack,
        conn_closed_call_back connClosedCallBack,
        void* data)
{
    struct server* server = malloc(sizeof(struct server));
    if (server == NULL) goto failed;
    
    struct acceptor* acceptor = acceptor_new(type, port);
    if (acceptor == NULL) goto failed;
    
    int nameLen = strlen(name);
    nameLen = nameLen < SERVER_NAME_MAXLEN ? nameLen : SERVER_NAME_MAXLEN;
    const char* serverName = malloc(nameLen + 1);
    strncpy(serverName, name, nameLen);
    struct event_loop* eventLoop = event_loop_new(serverName);
    if (eventLoop == NULL) goto failed;

    /* just create thread pool struct, haven't really start thread yet */
    struct thread_pool* threadPool = thread_pool_new(eventLoop, threadNum);
    if (threadPool == NULL) goto failed;

    server->acceptor = acceptor;
    server->eventLoop = eventLoop;

    server->connEstablishedCallBack = connEstablishedCallBack;
    server->connMsgReadCallBack = connMsgReadCallBack;
    server->connMsgWriteCallBack = connMsgWriteCallBack;
    server->connClosedCallBack = connClosedCallBack;

    server->threadPool = threadPool;
    server->threadNum = threadNum;

    if (data != NULL) server->data = data;
    else server->data = NULL;

    return server;

failed:
    // NOTE: actually no need to cleanup, just exit
    if (threadPool != NULL) thread_pool_cleanup(threadPool);
    if (eventLoop != NULL) event_loop_cleanup(eventLoop);
    if (acceptor != NULL) acceptor_cleanup(acceptor);
    if (server != NULL) free(server);
    return NULL;
}

void server_start(struct server* server)
{
    assertNotNULL(server);
    assertNotNULL(server->acceptor);

    // NOTE: server->threadPool may be NULL if threadNum = 0, thread_pool_run do nothing in this case
    thread_pool_run(server->threadPool);
    
    /* register channel for listening fd */
    struct acceptor* acceptor = server->acceptor;
    struct channel* chan = channel_new(acceptor->listen_fd, EVENT_READ, handle_connection_established, NULL, server);
    /* register EVENT_READ for acceptor->listen_fd to start accepting established client connection */
    event_loop_add_channel_event(server->event_loop, chan->fd, chan);
}

/* only used as acceptor EVENT_READ callback */
static int handle_connection_established(struct server* server)
{
    assertNotNULL(server);
    struct acceptor* acceptor = server->acceptor;
    

    /* TODO: only support ipv4 client addr for now, later support ipv6 */
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    int clientfd = accept(acceptor->listenfd, (SA*)&clientaddr, &addrlen);
    if (clientfd < 0) {     // may never happen ? not sure
        LOG(LT_DEBUG, "%s", strerror(errno));
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return 0;
        return -1;
    }

    LOG(LT_INFO, "new tcp connection established, socket fd : %d", clientfd);

    make_nonblocking(clientfd);

    struct event_loop* eventLoop = server_select_eventloop(server);

    /* local variable tcpConn is actually no need */
    struct tcp_connection* tcpConn = tcp_connection_new(clientfd, eventLoop,
                                                        server->connEstablishCallBack,
                                                        server->connMsgReadCallBack,
                                                        server->connMsgWriteCallBack,
                                                        server->connClosedCallBack);
    /* for callback use, httpserver */
    tcpConn->data = server->data;

    return 0;
}

/* select event loop of certain reactor thread */
static struct event_loop* server_select_eventloop(struct server* server)
{
    assertNotNULL(server);
    struct event_loop* selected;
    if (server->threadPool == NULL)
        selected = server->eventLoop;
    else
        selected = thread_pool_select_thread(server->threadPool);
    return selected;
}
