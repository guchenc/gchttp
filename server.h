#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include "acceptor.h"
#include "common.h"
#include "event_loop.h"
#include "thread_pool.h"
#include "tcp_connection.h"

/* server abstration */
struct server {
    /* server type, 0/1 for tcp/udp server */
    int type;
    /* acceptor for accepting connections */
    struct acceptor* acceptor;
    /* event loop of main-reactor thread  */
    struct event_loop* eventLoop;
    /* callbacks */
    conn_established_call_back connEstablishedCallBack;
    conn_msg_read_call_back connMsgReadCallBack;
    conn_msg_write_call_back connMsgWriteCallBack;
    conn_closed_call_back connClosedCallBack;
    /* holding sub-reactors thread info */
    struct thread_pool* threadPool;
    int threadNum;
    /* for callback use: httpserver */
    void* data;
};

/* create and init a tcp or udp server */
struct server*
server_new(const char* name, int type, int port, int threadNum,
        conn_established_call_back connEstablishedCallBack,
        conn_msg_read_call_back connMsgReadCallBack,
        conn_msg_write_call_back connMsgWriteCallBack,
        conn_closed_call_back connClosedCallBack,
        void* data);

/* start a server by registering EVENT_READ on listening fd */
void server_run(struct server* server);

#endif
