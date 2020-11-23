#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include "event_loop_thread.h"

struct event_loop;

/* sub-reactor thread pool */
struct thread_pool {
    struct event_loop* main_loop;
    int started;
    /* next sub-reactor thread idx to select */
    int next;
    struct event_loop_thread* threads;
    int nthread;
};

/**
 * create and initialize a thread pool
 */
struct thread_pool* thread_pool_new(struct event_loop* mainLoop, int nthread);

/**
 * create and run sub-reactor thread in thread pool
 * only called by main-reactor thread
 */
void thread_pool_run(struct thread_pool* threadPool);

/*
 * select a sub-reactor thread from thread pool
 * TODO: now polling, implement dynamic selection according to current connection count
 */
struct event_loop_thread* thread_pool_select_thread(struct thread_pool* threadPool);

/* clean up thread pool */
void thread_pool_cleanup(struct thread_pool* threadPool);

#endif
