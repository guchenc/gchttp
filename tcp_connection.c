#include "tcp_connection.h"

static void tcp_connection_set_peeraddr(struct tcp_connection* tcpConn, const struct sockaddr* peerAddr);

struct tcp_connection*
tcp_connection_new(int connFd, struct sockaddr* peerAddr, struct event_loop* eventLoop,
    conn_established_call_back connEstablishedCallBack,
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

    tcpConn->connEstablishedCallBack = connEstablishedCallBack;
    tcpConn->connMsgReadCallBack = connMsgReadCallBack;
    tcpConn->connMsgWriteCallBack = connMsgWriteCallBack;
    tcpConn->connClosedCallBack = connClosedCallBack;


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
    struct buffer* inBuffer = tcpConn->inBuffer;
    if (buffer_read_fd(inBuffer, tcpConn->channel->fd) > 0) {
        /* excute connection read callback */
        if (tcpConn->connMsgReadCallBack != NULL)
            tcpConn->connMsgReadCallBack(tcpConn);
    } else {
        /* NOTE: read EOF or error occured */
        handle_tcp_connection_closed(tcpConn);
    }
    return 0;
}

ssize_t handle_tcp_connection_write(struct tcp_connection* tcpConn)
{
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
    return 0;
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
    // TODO: figure out what work should be done to clean up close connection?
    close(tcpConn->channel->fd);
    free(tcpConn);
    return 0;
}

/**
 * NOTE
 * - when EVENT_WRITE on chan->fd is off(indicating that write() will not block) and outBuffer is empty, directy write to socket out buffer.
 *      - if write() write all bytes, error = 0 && nleft = 0, just return.
 *      - if write() return short count, error = 0 && nleft > 0, register EVENT_WRITE on fd and append left bytes to tcpConn->outBuffer, waiting for socket writable event.
 *      - if write() error occured, error = 1 && nleft > 0, register EVENT_WRITE on fd and append all bytes to tcpConn->outBuffer, sending in next dispatching round.
 * - when EVENT_WRITE on chan->fd is on(indicating that write() will block) or outBuffer is not empty(indicating that previous data have not been sent), append all bytes to tcpConn->outBuffer, sending later.
 * - tcp_connection_send() return number of bytes already being sent, leaving left bytes to framework
*/ 
ssize_t tcp_connection_send(struct tcp_connection* tcpConn, void* data, size_t size)
{
    size_t nleft = size;
    size_t nwritten = 0;
    int error = 0;

    struct buffer* outBuffer = tcpConn->outBuffer;
    struct channel* chan = tcpConn->channel;

    if (!channel_write_event_is_enabled(chan) && buffer_readable_size(outBuffer) == 0) {
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

    // NOTE: return value seems to be useless
    return nwritten;
}

ssize_t tcp_connection_send_buffer(struct tcp_connection* tcpConn, struct buffer* buff)
{
    size_t size = buffer_readable_size(buff);
    // TODO: tcp_connection_send() return bytes already being sent?
    size_t nwritten = tcp_connection_send(tcpConn, buff->data + buff->readIdx, size);
    buff->readIdx += nwritten;
    return nwritten;
}

void tcp_connection_shutdown(struct tcp_connection* tcpConn)
{
    if (shutdown(tcpConn->channel->fd, SHUT_WR) < 0) {
        // TODO: how to handle shutdown failure
        LOG(LT_WARN, "failed to shutdown socket(fd = %d) %s", tcpConn->channel->fd, strerror(errno));
    }
}
