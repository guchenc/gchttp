#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include "channel.h"
#include "event_loop.h"

typedef int (*conn_established_call_back)(struct tcp_connection* tcpConn);
typedef int (*conn_msg_read_call_back)(struct tcp_connection* tcpConn);
typedef int (*conn_msg_write_call_back)(struct tcp_connection* tcpConn);
typedef int (*conn_closed_call_back)(struct tcp_connection* tcpConn);

/* TCP连接的抽象 */
struct tcp_connection {
    struct event_loop* eventLoop; // event loop handling this connection
    struct channel* channel;      // conn fd and interested event
    struct sockaddr* peerAddr;  // store peer address

    struct buffer* inBuffer;  // application-level input buffer
    struct buffer* outBuffer; // application-level output buffer

    conn_established_call_back connCompletedCallBack;
    conn_msg_read_call_back msgReadCallBack;
    conn_msg_write_call_back msgWriteCallBack;
    conn_closed_call_back connClosedCallBack;

    void* data;     // for call back use: http_server
    void* request;  // for call back use
    void* response; // for call back use
};

/**
 * create a new tcp connection
 * key operations:
 *  - register channel EVENT_READ on connFd
 *  - setup input buffer and output buffer
 *  - excute connection established callback
 */
struct tcp_connection*
tcp_connection_new(int connFd, struct event_loop* eventLoop,
    conn_established_call_back connEstablishCallBack,
    conn_msg_read_call_back connMsgReadCallBack,
    conn_msg_write_call_back connMsgWriteCallBack,
    conn_closed_call_back connClosedCallBack);

/* using as socket fd EVENT_WRITE callback */
ssize_t handle_tcp_connection_write(struct tcp_connection* tcpConn);

/* using as socket fd EVENT_READ callback */
ssize_t handle_tcp_connection_read(struct tcp_connection* tcpConn);

/* application-level interface, write size bytes pointed by data to socket buffer */
ssize_t tcp_connection_send(struct tcp_connection* tcpConn, void* data, size_t size);

/* application-level interface, try to write all buffer readable bytes to socket buffer */
ssize_t tcp_connection_send_buffer(struct tcp_connection* tcpConn, struct buffer* buff);

/* handle connection closure by peer */
int handle_tcp_connection_closed(struct tcp_connection* tcpConn);

/* close tcp connection write end */
void tcp_connection_shutdown(struct tcp_connection* tcpConn);

#endif
