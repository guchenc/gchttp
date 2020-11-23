#include "event_loop_thread.h"

/* sub-reactor thread routine, initialize and start event loop */
static void* event_loop_thread_routine(void* arg)
{
    struct event_loop_thread* eventLoopThread = (struct event_loop_thread*)arg;

    pthread_mutex_lock(&eventLoopThread->mutex);
    struct event_loop* eventLoop = event_loop_new(eventLoopThread->threadName);
    if (eventLoop == NULL) {
        LOG(LT_WARN, "failed to initialize %s!", eventLoopThread->threadName);
        return NULL;
    }
    eventLoopThread->eventLoop = eventLoop;
    pthread_mutex_unlock(&eventLoopThread->mutex);
    pthread_cond_signal(&eventLoopThread->cond);

    LOG(LT_INFO, "%s initialized", eventLoopThread->threadName);

    event_loop_run(eventLoopThread->eventLoop);
    return NULL;
}

void event_loop_thread_init(struct event_loop_thread* eventLoopThread, int id)
{
    if (eventLoopThread == NULL) return;
    /* NOTE: event_loop have not been created */
    eventLoopThread->eventLoop = NULL;
    eventLoopThread->tid = 0; // actually no need, but just do it
    pthread_mutex_init(&eventLoopThread->mutex, NULL);
    pthread_cond_init(&eventLoopThread->cond, NULL);
    char* name = malloc(32);
    sprintf(name, "%s%d", SUB_REACTOR_PREFIX, id);
    eventLoopThread->threadName = name;
    eventLoopThread->connHandled = 0;
}

void event_loop_thread_run(struct event_loop_thread* eventLoopThread)
{
    if (eventLoopThread == NULL) return;

    pthread_create(&eventLoopThread->tid, NULL, &event_loop_thread_routine, eventLoopThread);

    pthread_mutex_lock(&eventLoopThread->mutex);
    while (eventLoopThread->eventLoop == NULL) { 
        /* waiting for created thread to be successfully initialized. */
        // TODO: handling thread creation failure
        pthread_cond_wait(&eventLoopThread->cond, &eventLoopThread->mutex);
    }
    pthread_mutex_unlock(&eventLoopThread->mutex);
}

void event_loop_thread_cleanup(struct event_loop_thread* eventLoopThread)
{
    assert(eventLoopThread != NULL);
    event_loop_cleanup(eventLoopThread->eventLoop);
    pthread_mutex_destroy(&eventLoopThread->mutex);
    pthread_cond_destroy(&eventLoopThread->cond);
    char name[32];
    strcpy(name, eventLoopThread->threadName);
    if (eventLoopThread->threadName != NULL) free(eventLoopThread->threadName);
    LOG(LT_INFO, "%s clean up successfully!", name);
}
