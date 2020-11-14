#include "event_loop.h"
#include <sys/poll.h>
#define INIT_POLL_SIZE 1024

struct poll_dispatcher_data {
    int event_count;
    int nfds;
    int realloc_copy;
    struct pollfd* fdarry;
    struct pollfd* fdarry_copy;
}

static void*
poll_init(struct event_loop* eventLoop);
static int poll_add(struct event_loop* eventLoop, struct channel* chan);
static int poll_del(struct event_loop* eventLoop, struct channel* chan);
static int poll_update(struct event_loop* eventLoop, struct channel* chan);
static int poll_dispatch(struct event_loop* eventLoop, struct timeval* timeout);
static void poll_clear(struct event_loop* eventLoop);

const struct event_dispatcher poll_dispatcher = {
    "poll",
    poll_init,
    poll_add,
    poll_del,
    poll_update,
    poll_dispatch,
    poll_clear,
};

// TODO: 并不需要通过返回值返回，可以通过参数直接赋值 eventLoop->dispatcher_data = pollDispatcherData
// TODO: 目前最多实现同时监听INIT_POLL_SIZE个fd上的事件，考虑实现fdarray的自动扩容
void* poll_init(struct event_loop* eventLoop)
{
    struct poll_dispatcher_data* pollDispatcherData = malloc(sizeof(struct poll_dispatcher_data));
    pollDispatcherData->fdarry = malloc(sizeof(struct pollfd) * INIT_POLL_SIZE);
    for (int i = 0; i < INIT_POLL_SIZE; i++)
        pollDispatcherData->fdarry[i].fd = -1;
    pollDispatcherData->event_count = 0;
    pollDispatcherData->nfds = 0;
    pollDispatcherData->realloc_copy = 0;
    pollDispatcherData->fdarry_copy = NULL;
    return pollDispatcherData;
}

// NOTE 重复添加会怎么样
int poll_add(struct event_loop* eventLoop, struct channel* chan)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    int events = 0;
    if (chan->events & EVENT_READ)
        events |= POLLRDNORM;
    if (chan->events & EVENT_WRITE)
        events |= POLLWRNORM;

    // 找到第一个可用的slot
    int i = 0;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        if (pollDispatcherData->fdarry[i].fd == -1) {
            pollDispatcherData->fdarry[i].fd = fd;
            pollDispatcherData->fdarry[i].events = events;
            break;
        }
    }
    if (i == INIT_POLL_SIZE) {
        printf("too many clients, just abort!\n");
    }
    return 0;
}

int poll_del(struct event_loop* eventLoop, struct channel* chan)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    int i = 0;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        if (pollDispatcherData->fdarry[i].fd == fd) {
            pollDispatcherData->fdarry[i].fd = -1;
            break;
        }
    }
    if (i == INIT_POLL_SIZE) {
        printf("poll_del cannot find fd %d\n", fd);
    }
    return 0;
}

int poll_update(struct event_loop* eventLoop, struct channel* chan)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    int events = 0;
    if (chan->events & EVENT_READ)
        events |= POLLRDNORM;
    if (chan->events & EVENT_WRITE)
        events |= POLLWRNORM;
    int i;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        if (pollDispatcherData->fdarry[i].fd == fd) {
            pollDispatcherData->fdarry[i].events = events;
            break;
        }
    }
    if (i == INIT_POLL_SIZE) {
        printf("poll_update cannot find fd %d\n", fd);
    }
    return 0;
}

int poll_dispatch(struct event_loop* eventLoop, struct timeval* timeout)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int nready = 0;
    int timewait = timeout->tv_sec * 1000;
    if ((nready = poll(pollDispatcherData->fdarry, INIT_POLL_SIZE, timewait)) < 0) {
        printf("poll failed!\n");
        return;
    }
    if (nready == 0)
        return 0; // 指定等待时间内没有事件发生，直接返回
    int i = 0;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        struct pollfd pollfd = pollDispatcherData->fdarry[i];
        if (pollfd.fd < 0)
            continue;             // 忽略fd=-1的pollfd
        if (pollfd.revents > 0) { // 若该fd上有事件发生，调用fd相应channel的回调函数
            if (pollfd.revents & POLLRDNORM) {
                channel_event_activate(eventLoop, pollfd.fd, EVENT_READ);
            }
            if (pollfd.revents & POLLWRNORM) {
                channel_event_activate(eventLoop, pollfd.fd, EVENT_WRITE);
            }
            if (--nready == 0)
                break; // 如果处理完产生事件的fd，提前跳出循环，没必要遍历剩余的pollfd
        }
    }
    return 0;
}

void poll_clear(struct event_loop* eventLoop)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    if (pollDispatcherData->fdarry != NULL) {
        free(pollDispatcherData->fdarry);
        pollDispatcherData->fdarry = NULL;
    }
    if (pollDispatcherData->fdarry_copy != NULL) {
        free(pollDispatcherData->fdarry_copy);
        pollDispatcherData->fdarry_copy = NULL;
    }
    free(pollDispatcherData);
    eventLoop->event_dispatcher_data = NULL;
}
