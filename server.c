#include "server.h"

/* extern struct tcp_connection*
 * tcp_connection_new(int connFd, struct sockaddr* peerAddr, struct event_loop* eventLoop,
 *         conn_established_call_back connEstablishCallBack,
 *         conn_msg_read_call_back connMsgReadCallBack,
 *         conn_msg_write_call_back connMsgWriteCallBack,
 *         conn_closed_call_back connClosedCallBack); */

static int handle_tcp_connection_established(struct server* server);
static struct event_loop* server_select_eventloop(struct server* server);

struct server*
server_new(const char* name, int type, int port, int threadNum,
        conn_established_call_back connEstablishedCallBack,
        conn_msg_read_call_back connMsgReadCallBack,
        conn_msg_write_call_back connMsgWriteCallBack,
        conn_closed_call_back connClosedCallBack,
        void* data)
{
    struct server* server = NULL;
    struct event_loop* eventLoop = NULL;
    struct acceptor* acceptor = NULL;
    struct thread_pool* threadPool = NULL;

    server = malloc(sizeof(struct server));
    if (server == NULL) goto failed;
    
    acceptor = acceptor_new(type, port);
    if (acceptor == NULL) goto failed;
    LOG(LT_INFO, "%s acceptor initialized", name);

    size_t len = strlen(name);
    int nameLen = len <= SERVER_NAME_MAXLEN ? len : SERVER_NAME_MAXLEN;
    char* serverName = malloc(nameLen + 1);
    strncpy(serverName, name, nameLen);
    serverName[nameLen] = '\0';

    eventLoop = event_loop_new(serverName);
    if (eventLoop == NULL) goto failed;

    /* just create thread_pool struct, have not really create thread yet */
    threadPool = thread_pool_new(eventLoop, threadNum);
    if (threadNum > 0 && threadPool == NULL) goto failed;
    if (threadNum == 0)
        LOG(LT_INFO, "%s being only i/o reactor thread", name);
    else
        LOG(LT_INFO, "%s thread pool(%d) initialized", name, threadNum);

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

void server_run(struct server* server)
{
    assertNotNULL(server);
    assertNotNULL(server->acceptor);

    // NOTE: server->threadPool may be NULL if threadNum = 0, thread_pool_run do nothing in this case
    thread_pool_run(server->threadPool);
    
    /* register channel for listening fd */
    struct acceptor* acceptor = server->acceptor;
    struct channel* chan = channel_new(acceptor->listen_fd, EVENT_READ, handle_tcp_connection_established, NULL, server);
    /* register EVENT_READ for acceptor->listen_fd to start accepting established client connection */
    event_loop_add_channel_event(server->eventLoop, chan->fd, chan);
    event_loop_run(server->eventLoop);
}

/* only used as acceptor EVENT_READ callback */
static int handle_tcp_connection_established(struct server* server)
{
    assertNotNULL(server);
    struct acceptor* acceptor = server->acceptor;

    /* TODO: only support ipv4 client addr for now, later support ipv6 */
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);

    int clientfd = accept(acceptor->listen_fd, (SA*)&clientaddr, &addrlen);
    if (clientfd < 0) {     // may never happen ? not sure
        LOG(LT_DEBUG, "%s", strerror(errno));
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return 0;
        return -1;
    }

    LOG(LT_INFO, "tcp connection established, socket fd = %d", clientfd);

    make_nonblocking(clientfd);

    struct event_loop* eventLoop = server_select_eventloop(server);
    LOG(LT_DEBUG, "select %s for handling i/o events on connection(fd = %d)", eventLoop->thread_name, clientfd);

    struct tcp_connection* tcpConn = tcp_connection_new(clientfd, (SA*)&clientaddr, eventLoop,
                                                        server->connEstablishedCallBack,
                                                        server->connMsgReadCallBack,
                                                        server->connMsgWriteCallBack,
                                                        server->connClosedCallBack);
    // register EVENT_READ on connFd
    event_loop_add_channel_event(tcpConn->eventLoop, clientfd, tcpConn->channel);

    // NOTE: execute connection established callback
    if (tcpConn->connEstablishedCallBack != NULL) {
        tcpConn->connEstablishedCallBack(tcpConn);
    }

    /* for callback use, httpserver */
    /* tcpConndata = server->data; */

    return 0;
}

/* select event loop of certain reactor thread */
static struct event_loop* server_select_eventloop(struct server* server)
{
    assertNotNULL(server);
    struct event_loop* selected = NULL;
    struct event_loop_thread* selectedThread = NULL;
    if (server->threadPool == NULL)
        selected = server->eventLoop;
    else {
        selectedThread = thread_pool_select_thread(server->threadPool);
        selected = selectedThread->eventLoop;
    }
    return selected;
}
