#include "event_loop.h"

/* 由reactor线程调用一次，初始化一个event_loop */
struct event_loop* event_loop_new(const char* thread_name)
{
    struct event_loop* eventLoop = malloc(sizeof(struct event_loop));
    if (eventLoop == NULL) goto failed;

    eventLoop->status = EVENT_LOOP_OVER;

    // 选择一个支持I/O复用作为dispatcher实现
#ifdef EPOLL_ENABLE
    eventLoop->eventDispatcher = &epoll_dispatcher;
#elif defined POLL_ENABLE
    eventLoop->eventDispatcher = &poll_dispatcher;
#elif defined SELECT_ENABLE
    eventLoop->eventDispatcher = &select_dispatcher;
#else
    eventLoop->eventDispatcher = NULL;
    LOG(LT_FATAL_ERROR, "OS dosen't support any dispatcher!");
    goto failed;
#endif
    eventLoop->event_dispatcher_data = eventLoop->eventDispatcher->init(eventLoop); 

    eventLoop->channelMap = chanmap_init();
    if (eventLoop->channelMap == NULL) goto failed;

    eventLoop->is_handling_pending = 0;
    eventLoop->pending_head = NULL;
    eventLoop->pending_tail = NULL;

    eventLoop->owner_thread_id = pthread_self();
    pthread_mutex_init(&eventLoop->mutex, NULL);
    pthread_cond_init(&eventLoop->cond, NULL);
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, eventLoop->socketPair) < 0) {
        LOG(LT_ERROR, "failed to create socketpair!");
        goto failed;
    }

    struct channel* chan = channel_create(event_loop->socketpair[1], EVENT_READ, handleWakeup, NULL, eventLoop);
    if (chan == NULL) goto failed;
    event_loop_add_channel_event(eventLoop, eventLoop->socketpair[1], chan);

    if (thread_name != NULL) {
        eventLoop->thread_name = thread_name;
    } else {
        eventLoop->thread_name = "Reactor main-thread";
    }

    return eventLoop;

failed:
    if (eventLoop != NULL) free(eventLoop);
    return NULL;
}

static int event_loop_do_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan, int type)
{
    pthread_mutex_lock(&eventLoop->mutex);
    event_loop_channel_buffer_nolock(eventLoop, fd, chan, type);
    pthread_mutex_unlock(&eventLoop->mutex);

    if (evenLoop->owner_thread_id != pthread_self()) {
        event_loop_wakeup(eventLoop);   // 如果eventloop不属于当前i/o reactor线程，则将对应线程唤醒，要求其立刻处理pending list
    } else {
        event_loop_handle_pending_channel(eventLoop);
    }
}

static int event_loop_channel_buffer_nolock(struct event_loop* eventLoop, int fd, struct channel* chan, int type)
{
    struct channel_element* chanElement = malloc(sizeof(struct channel_element));
    chanElement->type = type;
    chanElement->channel = chan;
    chanElement->next = NULL;
    if (eventLoop->head == NULL) {
        eventLoop->pending_head = eventLoop->pending_tail = chanElement;
    } else {
        eventLoop->pending_tail->next = chanElement;
        eventLoop->pending_tail = chanElement;
    }
}

int event_loop_add_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    return event_loop_do_channel_event(eventLoop, fd, chan, CHANNEL_OPT_ADD);
}

int event_loop_remove_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    return event_loop_do_channel_event(eventLoop, fd, chan, CHANNEL_OPT_DEL);
}

int event_loop_update_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    return event_loop_do_channel_event(eventLoop, fd, chan, CHANNEL_OPT_UPDATE);
}

// 由I/O reactor线程完成每次事件循环后调用，修改已注册套接字监听事件，之后进入新一轮循环
// 每次调用，处理完当前所有正在排队的channel操作事件，例如在事件分发器中注册新的套接字监听事件
int event_loop_handle_pending_channel(struct event_loop* eventLoop)
{
    pthread_mutex_lock(&eventLoop->mutex);
    eventLoop->is_handling_pending = 1;
    struct channel_element* chanElement = eventLoop->pending_head, *prev = NULL;
    while (chanElement != NULL) {
        struct channel* chan = chanElement->channel;
        int fd = chan->fd;
        if (chanElement->type == CHANNEL_OPT_ADD) {
            event_loop_handle_pending_add(eventLoop, fd, chan);
        } else if (chanElement->type == CHANNEL_OPT_DEL) {
            event_loop_handle_pending_del(eventLoop, fd, chan);
        } else if (chanElement->type == CHANNEL_OPT_UPDATE) {
            event_loop_handle_pending_update(eventLoop, fd, chan);
        }
        prev = chanElement;
        chanElement = chanElement->next;
        free(prev); // 释放动态内存
    }

    eventLoop->pending_head = eventLoop->pending_tail = NULL;
    event_loop->is_handling_pending = 0;
    pthread_mutext_unlock(&evenLoop->mutex);
    return 0;
}

int event_loop_handle_pending_add(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0) return 0;
    if (fd >= chanMap->nentries) {
        // TODO: 对channelMap进行扩容
        return 0;
    }

    // 第一次创建某个fd的channel时，将其插入channelmap，否则忽略，即便channel事件可能不同
    if (chanMap->entries[fd] == NULL) {
        chanMap->entries[fd] = chan;
        struct event_dispatcher* eventDispatcher =  eventLoop->event_dispatcher;
        eventDispatcher->add(eventLoop, chan);
        return 1;
    }
    return 0;
}

// in i/o reactor thread
int event_loop_handle_pending_remove(struct event_loop* eventLoop, int fd, struct channel* chan1)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0) return 0;
    if (fd >= chanMap->nentries) return -1;

    struct channel* chan = chanMap->entries[fd];
    struct event_dispatcher* eventDispatcher =  eventLoop->event_dispatcher;
    if (chan == NULL) return 0;
    int ret = 0;
    if (eventDispatcher->del(eventLoop, chan) == -1) ret = -1;
    else ret = 1;
    chanMap->entries[fd] = NULL;
    return ret;
}

int event_loop_handle_pending_update(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0 || fd >= chanMap->nentries) return 0;
    if (chanMap->entries[fd] == NULL) return -1;
    struct event_dispatcher* eventDispatcher =  eventLoop->event_dispatcher;
    eventDispatcher->update(eventLoop, chan);
}

// dispather派发完事件之后，调用该方法通知event_loop执行对应事件的相关callback方法
// res: EVENT_READ | EVENT_READ等
int channel_event_activate(struct event_loop* eventLoop, int fd, int event)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0 || fd >= chanMap->nentries) return 0;
    struct channel* chan = chanMap[fd];
    if (event & EVENT_READ)
        if (chan->eventReadCallBack != NULL) chan->eventReadCallBack(chan->data); 

    if (event & EVENT_WRITE)
        if (chan->eventWriteCallBack != NULL) chan->eventWriteCallBack(chan->data);
    return 0;
}

/* not sure, may have problem */
void event_loop_cleanup(struct event_loop* eventLoop)
{
    if (eventLoop == NULL) return;
    assert(pthread_mutex_trylock(eventLoop->mutex) == 0);   // make sure no thread hold this mutex
    event_dispatcher->clear();
    channel_map_cleanup(eventLoop->channelMap);
    assert(eventLoop->is_handling_pending == 0);
    pthread_mutex_destroy(&eventLoop->mutex);
    pthread_cond(&eventLoop->cond);
    close(eventLoop->socketPair[0]);
    close(eventLoop->socketPair[1]);
    if (eventLoop->thread_name != NULL) free(eventLoop->thread_name);
}

void event_loop_wakeup(struct event_loop* eventLoop)
{
    
}


int handle_wakeup(void* data)
{

}

/* 无线循环的事件分发器 */
int event_loop_run(struct event_loop* eventLoop)
{
    struct event_dispatcher* eventDispatcher = eventLoop->eventDispatcher;
    struct timval timeout;
    timeout.tv_sec = DISPATCH_TIMEOUT_SEC;

    while (eventLoop->status != EVENT_LOOP_OVER) {
        eventDispatcher->dispatch(eventLoop, &timeout);
        event_loop_handle_pending_channel(eventLoop);
    }
    return 0;
}
