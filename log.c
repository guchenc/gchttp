#include "log.h"

FILE* log_file = NULL; // 注意在程序退出时fclose

const char* LOG_LEVEL_STRS[5] = {
    "DEBUG",
    "INFO",
    "WRAN",
    "ERROR",
    "FATAL"
};

void server_log(int level, char* file, int line, char* fmt, ...)
{
    if (level < LT_DEBUG || level > LT_FATAL_ERROR)
        return;
    // TODO: 只输出等级大于等于当前log_level的日志
    time_t now_time;
    struct tm* time_info = NULL;
    char t_str[40];
    char msg_buff[1024];
    va_list ap;
    va_start(ap, fmt);

    now_time = time(NULL);
    time_info = localtime(&now_time);
    strftime(t_str, 40, "%Y-%m-%d %H:%M:%S", time_info);
    vsprintf(msg_buff, fmt, ap);
#ifdef ENABLE_CMDLOG
        printf("[%5s] %s %s:%-4d %s \n", LOG_LEVEL_STRS[level], t_str, file, line, msg_buff);
#endif
#ifdef ENABLE_FILELOG
    if (log_file == NULL) {
        log_file = fopen(LOG_FILE_NAME, "a+");
        if (log_file == NULL) {
            va_end(ap);
            return;
        }
    }
    fprintf(log_file, "[%5s] %s %s in %s:%d\n", LOG_LEVEL_STRS[level], t_str, msg_buff, file, line);
#endif
    va_end(ap);
}
