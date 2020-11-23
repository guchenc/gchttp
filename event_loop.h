#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H
#include "channel.h"
#include "channel_map.h"
#include "common.h"
#include "event_dispatcher.h"

#define DEFAULT_MAIN_REACTOR_NAME "main-reactor"

#define EVENT_LOOP_RUNNING 0
#define EVENT_LOOP_OVER 1

#define CHANNEL_OPT_ADD 0
#define CHANNEL_OPT_DEL 1
#define CHANNEL_OPT_UPDATE 2

#define DISPATCH_TIMEOUT_SEC 1

extern const struct event_dispatcher select_dispatcher;
extern const struct event_dispatcher poll_dispatcher;
extern const struct event_dispatcher epoll_dispatcher;

/* channel链表 */
// TODO: 待处理pending list可以改成链表实现的队列
// TODO: 每次向pending list中添加新的channel opt都需要malloc，处理玩之后又需要释放，比较占用时间，考虑使用一个队列维护事先分配好的可供使用的channel_element
struct channel_element {
    int type;
    struct channel* channel;
    struct channel_element* next;
};

struct channelopt_pending_list {
    int optcnt;
    struct channel_element* head;
    struct channel_element* tail;
};

/* reactor模型 */
struct event_loop {
    int status;

    /* 事件分发器，由具体的select/poll/epoll I/O复用实现 */
    const struct event_dispatcher* eventDispatcher;
    void* event_dispatcher_data; 
    /* 文件描述符和channel的映射，用于通过fd快速获得channel，进而快速找到相应事件的回调函数 struct channel* chan = channelMap[fd] */
    struct channel_map* channelMap; 

    /* 等待在事件分发器(select/poll/epoll)中修改的channel链表 串行无锁 */
    int is_handling_pending;
    struct channel_element* pending_head;
    struct channel_element* pending_tail;

    pthread_t owner_tid;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    /* 一对无名的相互连接的套接字，可用于全双共通信 */
    int socketPair[2]; 
    char* thread_name;
};

/* only called once by every reactor thread, create and init an event_loop */
struct event_loop* event_loop_new(char* thread_name);

int event_loop_run(struct event_loop* eventLoop);

int event_loop_add_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan);

int event_loop_remove_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan);

int event_loop_update_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan);

int event_loop_handle_pending_channel(struct event_loop* eventLoop);

int event_loop_handle_pending_add(struct event_loop* eventLoop, int fd, struct channel* chan);

int event_loop_handle_pending_del(struct event_loop* eventLoop, int fd, struct channel* chan);

int event_loop_handle_pending_update(struct event_loop* eventLoop, int fd, struct channel* chan);

void event_loop_cleanup(struct event_loop* eventLoop);

/* event_dispather检测到fd上的I/O事件后，调用该方法通知event_loop执行对应事件的相关callback方法，EVENT_READ | EVENT_WRITE等 */
int channel_event_activate(struct event_loop* eventLoop, int fd, int revent);

/* 每个reactor线程持有一个独立的event_loop，断言当前线程处理的是自身的event_loop */
void assertInOwnerThread(struct event_loop* eventLoop);

/* 判断当前处理event_loop的线程是否是其拥有者 */
int in_owner_thread(struct event_loop* eventLoop);

#endif
