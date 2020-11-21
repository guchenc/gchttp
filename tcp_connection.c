#include "tcp_connection.h"

struct tcp_connection*
tcp_connection_new(int connFd, struct sockaddr* peerAddr, struct event_loop* eventLoop,
    conn_established_call_back connEstablishCallBack,
    conn_msg_read_call_back connMsgReadCallBack,
    conn_msg_write_call_back connMsgWriteCallBack,
    conn_closed_call_back connClosedCallBack)
{
    assert(connFd > 0);
    assert(eventLoop != NULL);

    struct tcp_connection* tcpConn = malloc(sizeof(struct tcp_connection));
    if (tcpConn == NULL) goto failed;
    
    tcpConn->eventLoop = eventLoop;

    struct channel* chan = channel_new(connFd, EVENT_READ, handle_tcp_connection_read, handle_tcp_connection_write, tcpConn);
    if (chan == NULL) goto failed;
    tcpConn->channel = chan;

    tcp_connection_set_peeraddr(tcpConn, peerAddr);

    tcpConn->inBuffer = buffer_new();
    if (tcpConn->inBuffer == NULL) goto failed;
    tcpConn->outBuffer = buffer_new();
    if (tcpConn->outBuffer == NULL) goto failed;

    tcpConn->connEstablishCallBack = connEstablishCallBack;
    tcpConn->conn_msg_read_call_back = connMsgReadCallBack;
    tcpConn->conn_msg_write_call_back = connMsgWriteCallBack;
    tcpConn->connClosedCallBack = connClosedCallBack;

    // NOTE: execute connection established callback
    if (tcpConn->connEstablishCallBack != NULL) {
        tcpConn->connEstablishCallBack(tcpConn);
    }

    // register EVENT_READ on connFd
    event_loop_add_channel_event(tcpConn->eventLoop, connFd, tcpConn->channel);

    return tcpConn;

failed:
    // TODO: then how to deal with this failed connection
    LOG(LT_WARN, "failed to new tcp connection for socket fd %d", connFd);
    if (tcpConn->inBuffer != NULL) buffer_cleanup(tcpConn->inBuffer);
    if (tcpConn->outBuffer != NULL) buffer_cleanup(tcpConn->outBuffer);
    if (tcpConn->channel != NULL) free(tcpConn->channel);
    if (tcpConn->peerAddr != NULL) free(tcpConn->peerAddr);
    if (tcpConn != NULL) free(tcpConn);
    return NULL;
}

static void tcp_connection_set_peeraddr(struct tcp_connection* tcpConn, const struct sockaddr* peerAddr)
{
    struct sockaddr* addr;
    sa_family_t addrFamily = peerAddr->sa_family;
    /* ipv4, ipv6 supported */
    switch(addrFamily) {
        case AF_INET:
            addr = malloc(sizeof(struct sockaddr_in));
            memcpy(addr, peerAddr, sizeof(struct sockaddr_in));
            break;
        case AF_INET6:
            addr = malloc(sizeof(struct sockaddr_in6));
            memcpy(addr, peerAddr, sizeof(struct sockaddr_in6));
            break;
        default:
            addr = NULL;
            LOG(LT_WARN, "unsupported address family %d of socket %d", addrFamily, tcpConn->channel->fd);
    }
    tcpConn->peerAddr = addr;
}

ssize_t handle_tcp_connection_read(struct tcp_connection* tcpConn)
{
    struct tcp_connection* tcpConn = data;
    struct buffer* inBuffer = tcpConn->inBuffer;
    if (buffer_read_fd(inBuffer, tcpConn->channel->fd) > 0) {
        /* excute connection read callback */
        if (tcpConn->conn_msg_read_call_back() != NULL)
            tcpConn->connMsgReadCallBack(tcpConn);
    } else {
        /* read EOF or error occured */
        handle_tcp_connection_closed(tcpConn);
    }
}

ssize_t handle_tcp_connection_write(struct tcp_connection* tcpConn)
{
    struct tcp_connection* tcpConn = data;

    struct event_loop* eventLoop = tcpConn->eventLoop;
    /* every reactor thread only handles connections for which it's responsible */
    assertInOwnerThread(eventLoop);

    struct buffer* outBuffer = tcpConn->outBuffer;
    struct channel* chan = tcpConn->channel;

    size_t nleft = buffer_readable_size(outBuffer);
    /* write as much bytes as it can, non-blocking */
    ssize_t nwritten = write(chan->fd, &outBuffer->data[outBuffer->readIdx], nleft);
    if (nwritten > 0) {
        outBuffer->readIdx += nwritten;
        /* if there is no readable byte in buffer, remove EVENT_WRITE on corresponding channel */
        if (buffer_readable_size(outBuffer) == 0) {
            channel_write_event_disable(chan);
        }

        /* excute connection write callback */
        if (tcpConn->connMsgWriteCallBack != NULL)
            tcpConn->connMsgWriteCallBack(tcpConn);
    }
    // NOTE: how to deal with error, just let it be, EVENT_WRITE on corresponding channel is still on, next round of dispatcher will handle it
    
    /* if (nwritten < 0) {
     *     if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN) {
     *         LOG(LT_WARN, "failed to write to socket fd %d, %s", chan->fd, strerror(errno));
     *     }
     * } */
}

int handle_tcp_connection_closed(struct tcp_connection* tcpConn)
{
    assert(tcpConn != NULL);
    struct event_loop* eventLoop = tcpConn->eventLoop;
    assertInOwnerThread(eventLoop);

    struct channel* chan = tcpConn->channel;
    /* remove registered fd event */
    // NOTE: 如果已经close的fd在执行该函数之前被分配给新的连接并且注册了事件，之后继续执行，会发生什么？
    // 不会出现这种情况，虽然多个reactor线程共享打开文件表，但是各自持有独立的event_loop，channel_map
    event_loop_remove_channel_event(eventLoop, chan->fd, chan);

    /* excute connection closed callback */
    if (tcpConn->connClosedCallBack != NULL) {
        tcpConn->connClosedCallBack(tcpConn);
    }
}

ssize_t tcp_connection_send(struct tcp_connection* tcpConn, void* data, size_t size)
{
    size_t nleft = size;
    size_t nwritten = 0;
    int error = 0;

    struct buffer* outBuffer = tcpConn->outBuffer;
    struct channel* chan = tcpConn->channel;

    if (!channel_write_event_is_enabled(chan) && buffer_writeable_size(outBuffer) == 0) {
        nwritten = write(chan->fd, data, size);
        if (nwritten >= 0) {
            nleft -= nwritten;
        } else {
            nwritten = 0;
            // TODO: confused, figure it out
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                if (errno == EPIPE || errno == ECONNRESET)
                    error = 1;
            }
        }
    }

    /* no error occured and left bytes to be sent, hand over to framework */
    if (!error && nleft > 0) {
        buffer_append(outBuffer, data + nwritten, nleft);
        if (!channel_write_event_is_enabled(chan))
            channel_write_event_enable(chan);
    }

    return nwritten;
}

ssize_t tcp_connection_send_buffer(struct tcp_connection* tcpConn, struct buffer* buff)
{
    size_t size = buffer_readable_size(buff);
    // TODO: how to deal with short count
    size_t nwritten = tcp_connection_send(tcpConn, buff->data, size);
    buff->readIdx += nwritten;
    return nwritten;
}

void tcp_connection_shutdown(struct tcp_connection* tcpConn)
{
    if (shutdown(tcpConn->channel->fd, SHUT_WR) < 0) {
        // TODO: how to handle shutdown failure
        LOG(LT_WARN, "failed to shutdown socket(fd = %d) %s", tcpConn->channel->fd, streror(errno));
    }
}
