#ifndef LOG_H
#define LOG_H
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define LOG(level, format, arg...) server_log(level, __FILE__, __func__,  __LINE__, format, ##arg)
#define LOG_SYSCALL_ERROR() server_log(LT_WARN, __FILE__, __func__,  __LINE__, "%s", strerror(errno))

#define LT_DEBUG 0
#define LT_INFO 1
#define LT_WARN 2
#define LT_ERROR 3
#define LT_FATAL_ERROR 4
#define DISABLE_LOG 5

#define ENABLE_CMDLOG
#define ENABLE_FILELOG

#define LOG_FILE_NAME "gchttp.log"

void server_log(int level, char* file, const char* func, int line, char* fmt, ...);

#endif
