#include "event_loop.h"
#include "event_dispatcher.h"
#include <sys/poll.h>

#define INIT_POLL_SIZE 1024

struct poll_dispatcher_data {
    int event_count;
    int nfds;
    int realloc_copy;
    struct pollfd* fdarry;
    struct pollfd* fdarry_copy;
};

static void* poll_init(struct event_loop* eventLoop);
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

// TODO: no need to return pollDispatcherData, just eventLoop->dispatcher_data = pollDispatcherData
// TODO: currently, only support up to INIT_POOL_SIZE pollfd at a time, consider implementing automatic expansion for fdarray
void* poll_init(struct event_loop* eventLoop)
{
    struct poll_dispatcher_data* pollDispatcherData = malloc(sizeof(struct poll_dispatcher_data));
    if (pollDispatcherData == NULL) goto failed;
    pollDispatcherData->fdarry = malloc(sizeof(struct pollfd) * INIT_POLL_SIZE);
    if (pollDispatcherData->fdarry == NULL) goto failed;

    for (int i = 0; i < INIT_POLL_SIZE; i++)
        pollDispatcherData->fdarry[i].fd = -1;
    pollDispatcherData->event_count = 0;
    pollDispatcherData->nfds = 0;
    pollDispatcherData->realloc_copy = 0;
    pollDispatcherData->fdarry_copy = NULL;

    return pollDispatcherData;

failed:
    if (pollDispatcherData->fdarry != NULL) free(pollDispatcherData->fdarry);
    if (pollDispatcherData != NULL) free(pollDispatcherData);
    return NULL;
}

// NOTE: what if poll add certain fd for several times?
int poll_add(struct event_loop* eventLoop, struct channel* chan)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int fd = chan->fd;
    int events = 0;
    if (chan->events & EVENT_READ)
        events |= POLLRDNORM;
    if (chan->events & EVENT_WRITE)
        events |= POLLWRNORM;

    /* find first available slot for channel, actually may become bottleneck */
    int i = 0;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        if (pollDispatcherData->fdarry[i].fd == -1) {
            pollDispatcherData->fdarry[i].fd = fd;
            pollDispatcherData->fdarry[i].events = events;
            break;
        }
    }
    if (i == INIT_POLL_SIZE) {
        LOG(LT_WARN, "too many clients, no slot for new client(fd = %d), just abort!", fd);
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
        LOG(LT_WARN, "cannot find registered pollfd for fd = %d", fd);
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
        LOG(LT_WARN, "cannot find registered pollfd for fd = %d", fd);
    }
    return 0;
}

int poll_dispatch(struct event_loop* eventLoop, struct timeval* timeout)
{
    struct poll_dispatcher_data* pollDispatcherData = eventLoop->event_dispatcher_data;
    int nready = 0;
    int timewait = timeout->tv_sec * 1000;

    /* NOTE: where reactor thread will be actually blocked */
    if ((nready = poll(pollDispatcherData->fdarry, INIT_POLL_SIZE, timewait)) < 0) {    
        LOG(LT_WARN, "%s", strerror(errno));    // error occured (EAGAIN, EINIR, EINVAL), just return for next round
        return 0;
    }
    if (nready == 0)
        return 0; // no event happen to registered pollfds in timewait, just return

    LOG(LT_DEBUG, "%s poll returned, nready = %d\n", eventLoop->thread_name, nready);

    int i = 0;
    for (i = 0; i < INIT_POLL_SIZE; i++) {
        struct pollfd* pollfd = &pollDispatcherData->fdarry[i];
        if (pollfd->fd < 0)
            continue;             // ignore pollfd.fd == -1 
        if (pollfd->revents > 0) {  // event happen on this fd, excute callback function corresponding to that event
            if (pollfd->revents & POLLRDNORM) {
                channel_event_activate(eventLoop, pollfd->fd, EVENT_READ);
            }
            if (pollfd->revents & POLLWRNORM) {
                channel_event_activate(eventLoop, pollfd->fd, EVENT_WRITE);
            }
            if (--nready == 0)
                break; // just break when events of this round get handled
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
