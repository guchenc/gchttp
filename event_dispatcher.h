#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H
#include "channel.h"
#include "sys/time.h"
#include "event_loop.h"
/**
 * 事件分发器的抽象，可利用select，poll，epoll等I/O复用具体实现
 */
struct event_dispatcher {
    /* 采用的I/O复用名 */
    const char* name;   

    /* 初始化函数 NOTE：返回什么? */
    void* (*init)(struct event_loop* eventLoop);

    /* 通知dispatcher新增一个channel事件 */
    int (*add)(struct event_loop* eventLoop, struct channel* channel);

    /* 通知dispatcher删除一个channel事件 */
    int (*del)(struct event_loop* eventLoop, struct channel* channel);

    /* 通知dispatcher更新channel对应的事件 */
    int (*update)(struct event_loop* eventLoop, struct channel* channel);

    /* 实现事件分发，然后调用event_loop的event_activate方法执行信道相应事件的回调函数 */
    int (*dispatch)(struct event_loop* eventLoop, struct timeval* timeout);

    /* 释放动态内存 */
    void (*clear)(struct event_loop* eventLoop);

};

#endif
