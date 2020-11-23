#ifndef BUFFER_H
#define BUFFER_H
#include <stdlib.h>
#include "log.h"
#include <string.h>
#include <sys/uio.h>
#include <assert.h>

#define INIT_BUFFER_SIZE (1 << 16)  // 64kb
#define MAX_BUFFER_SIZE
#define CHEAP_PREPEND_SIZE 8     // append in front of data at low cost, space for time

/**
 * self-adaptable application-level buffer
 * non-thread-safe, but using safely
 *
 * | prependable |  readable | writeable |
 * ---------------------------------------
 * |             |  content  |           |     
 * ---------------------------------------
 * 0          readIdx     writeIdx    size()
 * invariants:
 * - 0 <= readIdx <= writeIdx <= size();
 * - prependable = readIdx;
 * - readable = writeIdx - readIdx;
 * - writeable = size() - writeIdx;
 */
struct buffer {
    char* data;
    size_t readIdx;
    size_t writeIdx;
    size_t size;
};

/* 分配并初始化一块应用层缓冲区 */
struct buffer* buffer_new();

/* 获取缓冲区当前可读字节数 */
size_t buffer_readable_size(struct buffer* buff);

/* 获取缓冲区当前可写空间字节大小 */
size_t buffer_writeable_size(struct buffer* buff);

/* 获取缓冲区当前首部可写空间字节大小 */
size_t buffer_prependable_size(struct buffer* buff);

/* 向缓冲区中写入由data指向的size个字节 */
size_t buffer_append(struct buffer* buff, const void* data, size_t size);

/* 向缓冲区中写入一个字符 */
size_t buffer_append_char(struct buffer* buff, char c);

/* 向缓冲区写入指定字符串 */
size_t buffer_append_string(struct buffer* buff, const char* str);

/* 从non-blocking fd中读取数据到缓冲区 */
ssize_t buffer_read_fd(struct buffer* buff, int fd);

/* 从缓冲区中读一个字符 */
char buffer_read_char(struct buffer* buff);

/* 在缓冲区中查询CRLF位置 */
char* buffer_find_CRLF(struct buffer* buff);

/* show content in buffer */
void buffer_show_content(struct buffer* buff);

/* 释放堆空间 */
void buffer_cleanup(struct buffer* buff);

#endif
