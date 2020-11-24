#include "event_loop.h"
#include "event_dispatcher.h"
#include <sys/select.h>
#include <sys/time.h>

struct select_dispatcher_data {
    int maxfd;
    int nfd;
    fd_set rset;
    fd_set wset;
    fd_set exceptset;
};

static void* select_init(struct event_loop* eventLoop);
static int select_add(struct event_loop* eventLoop, struct channel* chan);
static int select_del(struct event_loop* eventLoop, struct channel* chan);
static int select_update(struct event_loop* eventLoop, struct channel* chan);
static int select_dispatch(struct event_loop* eventLoop, struct timeval* timeout);
static void select_clear(struct event_loop* eventLoop);
static void update_maxfd(struct select_dispatcher_data* selectDispatcherData);

const struct event_dispatcher select_dispatcher = {
    "select",
    select_init,
    select_add,
    select_del,
    select_update,
    select_dispatch,
    select_clear
};

void* select_init(struct event_loop* eventLoop)
{
    struct select_dispatcher_data* selectDispatcherData = malloc(sizeof(struct select_dispatcher_data));
    if (selectDispatcherData == NULL) return NULL;

    selectDispatcherData->maxfd = -1;
    FD_ZERO(&selectDispatcherData->rset);
    FD_ZERO(&selectDispatcherData->wset);
    FD_ZERO(&selectDispatcherData->exceptset);

    return selectDispatcherData;
}

int select_add(struct event_loop* eventLoop, struct channel* chan)
{
    struct select_dispatcher_data* selectDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    if (chan->events & EVENT_READ)
        FD_SET(fd, &selectDispatcherData->rset);
    if (chan->events & EVENT_WRITE)
        FD_SET(fd, &selectDispatcherData->wset);
    if (fd > selectDispatcherData->maxfd) selectDispatcherData->maxfd = fd;
    return 0;
}

int select_del(struct event_loop* eventLoop, struct channel* chan)
{
    struct select_dispatcher_data* selectDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    FD_CLR(fd, &selectDispatcherData->rset);
    FD_CLR(fd, &selectDispatcherData->wset);

    /* deleted fd is maxfd, need to update maxfd */
    if (fd == selectDispatcherData->maxfd)
        update_maxfd(selectDispatcherData);

    return 0;
}

int select_update(struct event_loop* eventLoop, struct channel* chan)
{
    struct select_dispatcher_data* selectDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    /* NOTE: any proplem with calling FD_SET twice? */
    if (chan->events & EVENT_READ)
        FD_SET(fd, &selectDispatcherData->rset);
    else
        FD_CLR(fd, &selectDispatcherData->rset);

    if (chan->events & EVENT_WRITE)
        FD_SET(fd, &selectDispatcherData->wset);
    else
        FD_CLR(fd, &selectDispatcherData->wset);

    /* updated fd is not maxfd or still maxfd */
    if (fd != selectDispatcherData->maxfd ||
            FD_ISSET(fd, &selectDispatcherData->rset) ||
            FD_ISSET(fd, &selectDispatcherData->wset))
        return 0;

    /* updated fd was maxfd, need to update maxfd */
    update_maxfd(selectDispatcherData);

    return 0;
}

int select_dispatch(struct event_loop* eventLoop, struct timeval* timeout)
{
    struct select_dispatcher_data* selectDispatcherData = eventLoop->event_dispatcher_data;
    int timewait= timeout->tv_sec * 1000;
    int nready = 0;
    int maxfd = selectDispatcherData->maxfd;
    fd_set ready_rset = selectDispatcherData->rset;
    fd_set ready_wset = selectDispatcherData->wset;
    if ((nready = select(maxfd + 1, &ready_rset, &ready_wset, NULL, timeout)) < 0) {
        LOG(LT_WARN, "%s", strerror(errno));
        return 0;
    }
    if (nready == 0) return 0;
    LOG(LT_DEBUG, "%s select returned, nready = %d\n", eventLoop->thread_name, nready);

    for (int i = 0; i <= maxfd; i++) {
        if (FD_ISSET(i, &ready_rset)) {
            channel_event_activate(eventLoop, i, EVENT_READ);
            nready--;
        }
        if (FD_ISSET(i, &ready_wset)) {
            channel_event_activate(eventLoop, i, EVENT_WRITE);
            nready--;
        }
        if (nready == 0) break;
    }
    return 0;
}

void select_clear(struct event_loop* eventLoop)
{
    /* actually no cleanup work need to be done */
}

static void update_maxfd(struct select_dispatcher_data* selectDispatcherData)
{
    int old_maxfd = selectDispatcherData->maxfd;
    int i;
    for (i = old_maxfd; i >= 0; i--) {
        if (FD_ISSET(i, &selectDispatcherData->rset) || FD_ISSET(i, &selectDispatcherData->wset)) {
            selectDispatcherData->maxfd = i;
            break;
        }
    }
    if (i < 0) selectDispatcherData->maxfd = -1;
}
