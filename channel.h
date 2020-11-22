#ifndef CHANNEL_H
#define CHANNEL_H
#include <stdlib.h>

/* 信道等待发生的事件类型 */
#define EVENT_TIMEOUT 0x01
#define EVENT_READ 0x02
#define EVENT_WRITE 0x04 
#define EVENT_SIGNAL 0x08

// NOTE: forward-declaration, avoid circular including
struct event_loop;

/* 定义函数指针别名主要是为了方便传参 */
typedef int (*event_read_callback)(void* data);
typedef int (*event_write_callback)(void* data);

struct channel {
    int fd;                                // 该信道所属描述符
    int events;                            // 表示信道的监听事件类型
    event_read_callback eventReadCallBack;  // 该信道的读事件回调函数
    event_write_callback eventWriteCallBack; // 该信道的写事件回调函数
    void* data;                            // NOTE: 回调数据，event_loop/tcp_server/tcp_connection 
};

extern int event_loop_update_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan);

/* 创建一个新信道 */
struct channel* channel_new(int fd, int events, event_read_callback eventReadCallBack, event_write_callback eventWriteCallBack, void* data);

/* 判断一个信道的写事件监听是否开启 */
int channel_write_event_is_enabled(struct channel* chan);

/* 开启一个信道的写事件 */
int channel_write_event_enable(struct channel* chan);

/* 关闭一个信道的写事件 */
int channel_write_event_disable(struct channel* chan);

#endif
