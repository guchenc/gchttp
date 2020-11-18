#include "channel.h"

struct channel* channel_new(int fd, int events, event_read_callback eventReadCallBack, event_write_callback eventWriteCallBack, void* data) 
{
    struct channel* chan = malloc(sizeof(struct channel));
    if (chan == NULL) return NULL;
    chan->fd = fd;
    chan->events = events;
    chan->eventReadCallBack = eventReadCallBack;
    chan->eventWriteCallBack = eventWriteCallBack;
    chan->data = data;
    return chan;
}

int channel_write_event_is_enabled(struct channel* chan)
{
    return chan->events & EVENT_WRITE;
}

int channel_write_event_enable(struct channel* chan)
{
    struct event_loop* eventLoop = (struct event_loop*)chan->data;
    chan->events = chan->events | EVENT_WRITE;
    event_loop_update_channel_event(eventLoop, chan->fd, chan);
}

int channel_write_event_disable(struct channel* channel)
{
    struct event_loop* eventLoop = (struct event_loop*)chan->data;
    chan->events = chan->events & ~EVENT_WRITE;
    event_loop_update_channel_event(eventLoop, chan->fd, chan);
}
