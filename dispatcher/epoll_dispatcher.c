#include "event_loop.h"
#include "event_dispatcher.h"
#include <sys/epoll.h>

#define MAXEVENTS 128    // max ready event can be returned per epoll_wait

struct epoll_dispatcher_data {
    int efd;
    int nfds;
    struct epoll_event* readylist;
};

static void* epoll_init(struct event_loop* eventLoop);
static int epoll_add(struct event_loop* eventLoop, struct channel* chan);
static int epoll_del(struct event_loop* eventLoop, struct channel* chan);
static int epoll_update(struct event_loop* eventLoop, struct channel* chan);
static int epoll_dispatch(struct event_loop* eventLoop, struct timeval* timeout);
static void epoll_clear(struct event_loop* eventLoop);

const struct event_dispatcher epoll_dispatcher = {
    "epoll",
    epoll_init,
    epoll_add,
    epoll_del,
    epoll_update,
    epoll_dispatch,
    epoll_clear,
};

void* epoll_init(struct event_loop* eventLoop)
{
    struct epoll_dispatcher_data* epollDispatcherData = malloc(sizeof(struct epoll_dispatcher_data));
    if (epollDispatcherData == NULL) goto failed;

    int efd = epoll_create1(0);
    if (efd < 0) goto failed;

    struct epoll_event* readylist = malloc(MAXEVENTS * sizeof(struct epoll_event));
    if (readylist == NULL) goto failed;

    epollDispatcherData->efd = efd;
    epollDispatcherData->nfds = MAXEVENTS;
    epollDispatcherData->readylist = readylist;

    return epollDispatcherData;

failed:
    if (readylist != NULL) free(readylist);
    if (epollDispatcherData != NULL) free(epollDispatcherData);
    if (efd > 0) close(efd);
    return NULL;
}

int epoll_add(struct event_loop* eventLoop, struct channel* chan)
{
    struct epoll_dispatcher_data* epollDispatcherData = eventLoop->event_dispatcher_data;
    struct epoll_event ev;

    int fd = chan->fd;
    uint32_t events = 0;
    if (chan->events & EVENT_READ)
        events |= EPOLLIN;      // TODO: default level-triggered, supported edge-triggered
    if (chan->events & EVENT_WRITE)
        events |= EPOLLOUT;

    ev.data.fd = fd;
    ev.events = events;

    // NOTE: number of fd that can be added is limited by max_user_watches(1641533)
    if (epoll_ctl(epollDispatcherData->efd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        LOG(LT_WARN, "%s", strerror(errno));
        return -1;
    }

    return 0;
}

int epoll_del(struct event_loop* eventLoop, struct channel* chan)
{
    struct epoll_dispatcher_data* epollDispatcherData = eventLoop->event_dispatcher_data;
    if (epoll_ctl(epollDispatcherData->efd, EPOLL_CTL_DEL, chan->fd, NULL) < 0) {
        LOG(LT_WARN, "%s", strerror(errno));
        return -1;
    }
    return 0;
}

int epoll_update(struct event_loop* eventLoop, struct channel* chan)
{
    struct epoll_dispatcher_data* epollDispatcherData = eventLoop->event_dispatcher_data;
    struct epoll_event ev;

    int fd = chan->fd;
    uint8_t events = 0;
    if (chan->events & EVENT_READ)
        events |= EPOLLIN;
    if (chan->events & EVENT_WRITE)
        events |= EPOLLOUT;
    ev.data.fd = fd;
    ev.events = events;

    if (epoll_ctl(epollDispatcherData->efd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        LOG(LT_WARN, "%s", strerror(errno));
        return -1;
    }

    return 0;
}

int epoll_dispatch(struct event_loop* eventLoop, struct timeval* timeout)
{
    struct epoll_dispatcher_data* epollDispatcherData = eventLoop->event_dispatcher_data;
    int timewait = timeout->tv_sec * 1000;
    int nready = 0;
    if ((nready = epoll_wait(epollDispatcherData->efd, epollDispatcherData->readylist, epollDispatcherData->nfds, timewait)) < 0) {
        LOG(LT_WARN, "%s", strerror(errno));
        return -1;
    }
    if (nready == 0) return 0;

    for (int i = 0; i < nready; i++) {
        struct epoll_event* ev = &epollDispatcherData->readylist[i];
        int fd = ev->data.fd;
        uint32_t events = ev->events;

        // TODO: what's the difference between these event
        if (events & EPOLLIN || events & EPOLLERR || events & EPOLLHUP)
            channel_event_activate(eventLoop, fd, EVENT_READ);
        if (events & EPOLLOUT)
            channel_event_activate(eventLoop, fd, EVENT_WRITE);
    }
    return 0;
}


void epoll_clear(struct event_loop* eventLoop)
{
    struct epoll_dispatcher_data* epollDispatcherData = eventLoop->event_dispatcher_data;
    if (epollDispatcherData->efd > 0) close(epollDispatcherData->efd);
    if (epollDispatcherData->readylist != NULL) free(epollDispatcherData->readylist);
    if (epollDispatcherData != NULL) free(epollDispatcherData);
    eventLoop->event_dispatcher_data = NULL;
}
