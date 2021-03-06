#include "event_loop.h"

static int event_loop_channel_buffer_nolock(struct event_loop* eventLoop, int fd, struct channel* chan, int type);
static void event_loop_wakeup(struct event_loop* eventLoop);
static int handle_wakeup(void* data);

struct event_loop* event_loop_new(char* thread_name)
{
    struct event_loop* eventLoop = malloc(sizeof(struct event_loop));
    if (eventLoop == NULL) goto failed;

    eventLoop->status = EVENT_LOOP_OVER;

    /* select one supported I/O multiplexing as implementation of event dispatcher */
#ifdef EPOLL_ENABLED
    eventLoop->eventDispatcher = &epoll_dispatcher;
#elif defined POLL_ENABLED
    eventLoop->eventDispatcher = &poll_dispatcher;
#elif defined SELECT_ENABLED
    eventLoop->eventDispatcher = &select_dispatcher;
#else
    eventLoop->eventDispatcher = NULL;
    LOG(LT_FATAL_ERROR, "OS dosen't support any dispatcher!");
    goto failed;
#endif
    eventLoop->event_dispatcher_data = eventLoop->eventDispatcher->init(eventLoop); 
    if (eventLoop->event_dispatcher_data == NULL) goto failed;
    LOG(LT_INFO, "using %s as event dispatcher", eventLoop->eventDispatcher->name);

    eventLoop->channelMap = chanmap_new(sizeof(struct channel));
    if (eventLoop->channelMap == NULL) goto failed;

    eventLoop->is_handling_pending = 0;
    eventLoop->pending_head = NULL;
    eventLoop->pending_tail = NULL;

    eventLoop->owner_tid = pthread_self();
    pthread_mutex_init(&eventLoop->mutex, NULL);
    pthread_cond_init(&eventLoop->cond, NULL);
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, eventLoop->socketPair) < 0) {
        LOG(LT_ERROR, "failed to create socketpair!");
        goto failed;
    }

    struct channel* chan = channel_new(eventLoop->socketPair[1], EVENT_READ, handle_wakeup, NULL, eventLoop);
    if (chan == NULL) goto failed;
    event_loop_add_channel_event(eventLoop, eventLoop->socketPair[1], chan);

    if (thread_name != NULL) {
        eventLoop->thread_name = thread_name;
    } else {
        eventLoop->thread_name = DEFAULT_MAIN_REACTOR_NAME;
    }

    return eventLoop;

failed:
    if (eventLoop != NULL) free(eventLoop);
    return NULL;
}

static int event_loop_do_channel_event(struct event_loop* eventLoop, int fd, struct channel* chan, int type)
{
    /* must lock eventLoop, because main-reactor thread and the sub-reacotor thread it belongs to may access it at same time */
    pthread_mutex_lock(&eventLoop->mutex);
    event_loop_channel_buffer_nolock(eventLoop, fd, chan, type);
    pthread_mutex_unlock(&eventLoop->mutex);

    /* serial lock-free */
    if (in_owner_thread(eventLoop)) {
        event_loop_handle_pending_channel(eventLoop);
    } else {
        /* eventLoop doesn't belong to cur thread, wake up corresponding thread so as to update registered events immediately */
        event_loop_wakeup(eventLoop);   
    }
    return 0;
}

static int event_loop_channel_buffer_nolock(struct event_loop* eventLoop, int fd, struct channel* chan, int type)
{
    struct channel_element* chanElement = malloc(sizeof(struct channel_element));
    chanElement->type = type;
    chanElement->channel = chan;
    chanElement->next = NULL;
    if (eventLoop->pending_head == NULL) {
        eventLoop->pending_head = eventLoop->pending_tail = chanElement;
    } else {
        eventLoop->pending_tail->next = chanElement;
        eventLoop->pending_tail = chanElement;
    }
    return 0;
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
    eventLoop->is_handling_pending = 0;
    pthread_mutex_unlock(&eventLoop->mutex);
    return 0;
}

int event_loop_handle_pending_add(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0) return 0;
    if (fd >= chanMap->nentry) {
        // TODO: 对channelMap进行扩容
        return 0;
    }

    // 第一次创建某个fd的channel时，将其插入channelmap，否则忽略，即便channel事件可能不同
    if (chanMap->entries[fd] == NULL) {
        chanMap->entries[fd] = chan;
        eventLoop->eventDispatcher->add(eventLoop, chan);
        return 1;
    }
    return 0;
}

// in i/o reactor thread
int event_loop_handle_pending_del(struct event_loop* eventLoop, int fd, struct channel* chan1)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0) return 0;
    if (fd >= chanMap->nentry) return -1;

    struct channel* chan = chanMap->entries[fd];
    if (chan == NULL) return 0;
    int ret = 0;
    if (eventLoop->eventDispatcher->del(eventLoop, chan) == -1) ret = -1;
    else ret = 1;
    chanMap->entries[fd] = NULL;
    return ret;
}

int event_loop_handle_pending_update(struct event_loop* eventLoop, int fd, struct channel* chan)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0 || fd >= chanMap->nentry) return 0;
    if (chanMap->entries[fd] == NULL) return -1;
    eventLoop->eventDispatcher->update(eventLoop, chan);
    return 0;
}

// dispather派发完事件之后，调用该方法通知event_loop执行对应事件的相关callback方法
// res: EVENT_READ | EVENT_READ等
int channel_event_activate(struct event_loop* eventLoop, int fd, int event)
{
    struct channel_map* chanMap = eventLoop->channelMap;
    if (fd < 0 || fd >= chanMap->nentry) return 0;
    struct channel* chan = chanMap->entries[fd];
    if (event & EVENT_READ)
        if (chan->eventReadCallBack != NULL) chan->eventReadCallBack(chan->data); 

    if (event & EVENT_WRITE)
        if (chan->eventWriteCallBack != NULL) chan->eventWriteCallBack(chan->data);
    return 0;
}

/* NOTE: not sure, may have problem */
void event_loop_cleanup(struct event_loop* eventLoop)
{
    if (eventLoop == NULL) return;
    assert(pthread_mutex_trylock(&eventLoop->mutex) == 0);   // make sure no thread hold this mutex
    eventLoop->eventDispatcher->clear(eventLoop);
    chanmap_cleanup(eventLoop->channelMap);
    assert(eventLoop->is_handling_pending == 0);
    pthread_mutex_destroy(&eventLoop->mutex);
    pthread_cond_destroy(&eventLoop->cond);
    close(eventLoop->socketPair[0]);
    close(eventLoop->socketPair[1]);
    if (eventLoop->thread_name != NULL) free(eventLoop->thread_name);
}

/**
 * when new channel is ready to be registered, reactor thread may block on dispather->dispatch(). 
 * in this case, main reactor thread write one byte to socketPair[0] to trigger EVENT_READ on registered socketPair[1] so that sub-reactor thread will wake up immediately instead of waking up after timeout.
 */
static void event_loop_wakeup(struct event_loop* eventLoop)
{
    char c = 'a';
    ssize_t n = write(eventLoop->socketPair[0], &c, 1);
    if (n < 0)
        LOG(LT_WARN, "failed to wake up sub-reacotr thread %s", eventLoop->thread_name);
}

static int handle_wakeup(void* data)
{
    struct event_loop* eventLoop = data;
    char c;
    ssize_t n = read(eventLoop->socketPair[1], &c, 1);
    if (n < 0) {
        LOG(LT_WARN, "sub-reactor thread %s failed to wake up by reading socketpair", eventLoop->thread_name);
        return -1;
    }
    LOG(LT_DEBUG, "thread %s wakes up", eventLoop->thread_name);
    return 0;
}

/* infinite loop for event dispatcher */
int event_loop_run(struct event_loop* eventLoop)
{
    struct timeval timeout;
    timeout.tv_sec = DISPATCH_TIMEOUT_SEC;

    eventLoop->status = EVENT_LOOP_RUNNING;

    LOG(LT_INFO, "%s start event looping ...", eventLoop->thread_name);
    while (eventLoop->status != EVENT_LOOP_OVER) {
        LOG(LT_DEBUG, "%s begin event dispatching ...", eventLoop->thread_name);
        eventLoop->eventDispatcher->dispatch(eventLoop, &timeout);
        event_loop_handle_pending_channel(eventLoop);
    }
    return 0;
}

void assertInOwnerThread(struct event_loop* eventLoop)
{
    assert(pthread_equal(pthread_self(), eventLoop->owner_tid) != 0);
}

int in_owner_thread(struct event_loop* eventLoop)
{
    return pthread_equal(pthread_self(), eventLoop->owner_tid);
}
