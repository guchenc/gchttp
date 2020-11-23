#include "server.h"

int onClientConnected(struct tcp_connection* tcpConn)
{
    printf("callback of client connection(fd=%d, i/o handle thread=%s) established\n", tcpConn->channel->fd, tcpConn->eventLoop->thread_name);
    return 0;
}

int onClientMsgRecieved(struct tcp_connection* tcpConn)
{
    printf("callback of recieving message from client(fd=%d, i/o handle thread=%s)\n", tcpConn->channel->fd, tcpConn->eventLoop->thread_name);
    struct buffer* inBuffer = tcpConn->inBuffer;
    struct buffer* output = buffer_new();
    size_t size = buffer_readable_size(inBuffer);
    for (int i = 0; i < size; i++) {
        buffer_append_char(output, buffer_read_char(inBuffer));
    }
    buffer_show_content(output);
    tcp_connection_send_buffer(tcpConn, output);
    return 0;
}

int onClientMsgSent(struct tcp_connection* tcpConn)
{
    printf("callback of sending message to client(fd = %d, i/o handle thread=%s)\n", tcpConn->channel->fd, tcpConn->eventLoop->thread_name);
    return 0;
}

int onClientDisconnected(struct tcp_connection* tcpConn)
{
    printf("callback of client connection(fd=%d, i/o handle thread=%s) disconnected\n", tcpConn->channel->fd, tcpConn->eventLoop->thread_name);
    return 0;
}


int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("usage: ./gc_tcpserver <PORT>\n");
        return -1;
    }
    
    struct server* tcpServer = server_new("main-reactor", TCP_SERVER, atoi(argv[1]), 3,
            onClientConnected, onClientMsgRecieved, onClientMsgSent, onClientDisconnected, NULL);
    LOG(LT_INFO, "server initialized successfully, main thread: %s", tcpServer->eventLoop->thread_name);
    server_run(tcpServer);
    LOG(LT_INFO, "server exit successfully");
    return 0;
}
