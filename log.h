#ifndef LOG_H
#define LOG_H
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define LOG(level, format, arg...) server_log(level, __FILE__, __LINE__, format, ##arg)

#define LT_DEBUG 0
#define LT_INFO 1
#define LT_WARN 2
#define LT_ERROR 3
#define LT_FATAL_ERROR 4
#define DISABLE_LOG 5

#define ENABLE_CMDLOG
#define ENABLE_FILELOG

#define LOG_FILE_NAME "gchttp.log"

void server_log(int level, char* file, int line, char* fmt, ...);

#endif
