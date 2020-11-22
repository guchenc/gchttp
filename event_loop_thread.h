#ifndef EVENT_LOOP_THREAD_H
#define EVENT_LOOP_THREAD_H
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "event_loop.h"

struct event_loop;

/* 描述一个sub-reactor线程，只负责套接字I/O处理 */
struct event_loop_thread {
    /* 每个sub-reactor线程持有一个event_loop结构体，维护其负责处理的连接 */
    struct event_loop* eventLoop;
    pthread_t tid;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    char* threadName;
    unsigned long connHandled;
};

/**
 * 初始化已经分配好内存的event_loop_thread结构体
 * 由main-reactor线程调用
 */
void event_loop_thread_init(struct event_loop_thread* eventLoopThread,int id);

/**
 * 启动一个sub-reactor线程，开始事件轮询
 * 由main-reactor线程调用
 */
void event_loop_thread_run(struct event_loop_thread* eventLoopThread);

/*
 * 清理并释放资源
 */
void event_loop_thread_cleanup(struct event_loop_thread* eventLoopThread);

#endif
