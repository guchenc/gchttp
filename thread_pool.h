#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include "event_loop.h"
#include "event_loop_thread.h"

/* sub-reacotr线程池 */
struct thread_pool {
    struct event_loop* main_loop;
    int started;
    /* 处理下一个新连接的线程索引 */
    int next;
    struct event_loop_thread* threads;
    int nthread;
};

/**
 * 创建并初始化一个sub-reactor线程池
 */
struct thread_pool* thread_pool_new(struct event_loop* mainLoop, int nthread);

/**
 * 启动线程池中的sub-reactor线程
 * 由main-reactor线程调用
 */
void thread_pool_run(struct thread_pool* threadPool);

/*
 * 从线程池中取一个sub-reactor
 * TODO: 现为轮询，之后根据sub-reactor当前处理的连接数动态选择
 */
struct event_loop_thread* thread_pool_select_thread(struct thread_pool* threadPool);

/* clean up thread pool */
void thread_pool_cleanup(struct thread_pool* threadPool);

#endif
