#include "thread_pool.h"
#include <assert.h>

struct thread_pool* thread_pool_new(struct event_loop* mainLoop, int nthread)
{
    if (nthread <= 0) return NULL;

    struct thread_pool* threadPool = malloc(sizeof(struct thread_pool));
    if (threadPool == NULL) goto failed;

    threadPool->main_loop = mainLoop;
    threadPool->started = 0;
    threadPool->nthread = nthread;
    threadPool->next = 0;

    threadPool->threads = malloc(sizeof(struct event_loop_thread) * nthread);
    if (threadPool->threads == NULL) goto failed;

    for (int i = 0; i < threadPool->nthread; i++)
        event_loop_thread_init(&threadPool->threads[i], i);

    return threadPool;

failed:
    if (threadPool->threads != NULL) free(threadPool->threads);
    if (threadPool != NULL) free(threadPool);
    LOG(LT_WARN, "failed to created sub-reactor thread pool!");
    return NULL;
}

void thread_pool_run(struct thread_pool* threadPool)
{
    if (threadPool == NULL) return;

    int nthread = threadPool->nthread;
    if (nthread <=  0) return;

    for (int i = 0; i < nthread; i++)
        event_loop_thread_run(&threadPool->threads[i]);
    threadPool->started = 1;
    LOG(LT_INFO, "thread pool run successfully!");
}

struct event_loop_thread* thread_pool_select_thread(struct thread_pool* threadPool)
{
    assert(threadPool != NULL);
    assert(threadPool->nthread > 0);
    int selected = threadPool->next;
    threadPool->next = (selected + 1) % threadPool->nthread;
    return &threadPool->threads[selected];
}

void thread_pool_cleanup(struct thread_pool* threadPool)
{
    if (threadPool == NULL) return;
    // TODO: how to deal with thread
}
