#include "event_loop_thread.h"
#include "event_loop.h"
#include <assert.h>

void event_loop_thread_init(struct event_loop_thread* eventLoopThread, int id)
{
    if (eventLoopThread == NULL) return;
    eventLoopThread->eventLoop = NULL;
    eventLoopThread->tid = 0; // actually no need, but just do it
    pthread_mutex_init(&eventLoopThread->mutex, NULL);
    pthread_cond_init(&eventLoopThread->cond, NULL);
    char* name = malloc(32);
    sprintf(name, "Sub-Reactor-%d", id);
    eventLoopThread->threadName = name;
    eventLoopThread->connHandled = 0;
}

void event_loop_thread_run(struct event_loop_thread* eventLoopThread)
{
    if (eventLoopThread == NULL) return;

    pthread_create(&eventLoopThread->tid, NULL, &event_loop_thread_routine, eventLoopThread);

    pthread_mutex_lock(eventLoopThread->mutex);
    while (eventLoopThread->eventLoop == NULL) { 
        // waiting for created thread to be successfully initialized.
        pthread_cond_wait(eventLoopThread->cond, eventLoopThread->mutex);
    }
    pthread_mutex_unlock(eventLoopThread->mutex);

    LOG(LT_INFO, "%s start successfully!", eventLoopThread->threadName);
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

/* sub-reactor线程：初始化event_loop之后进入事件循环 */
static void* event_loop_thread_routine(void* arg)
{
    struct event_loop_thread* eventLoopThread = (struct event_loop_thread*)arg;

    pthread_mutex_lock(&eventLoopThread->mutex);
    struct event_loop* eventLoop = event_loop_new(eventLoopThread->threadName);
    if (eventLoop == NULL) {
        LOG(LT_WARN, "failed to initialize %s!", eventLoopThread->threadName);
        return NULL;
    }
    pthread_mutex_unlock(&eventLoopThread->mutex);
    pthread_cond_signal(&eventLoopThread->cond);

    LOG(LT_INFO, "%s initialized successfully!", eventLoopThread->threadName);

    event_loop_run(eventLoopThread->eventLoop);
}

