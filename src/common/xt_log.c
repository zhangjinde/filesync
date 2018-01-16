/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_log.h
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2016.12.07
 * Description:    日志处理
*************************************************/

#include "xt_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "cjson\cJSON.h"

#ifdef WINDOWS
#include <time.h>
#include <stdint.h>
#include <windows.h>

int gettimeofday(struct timeval *tv, void *tz)
{
    FILETIME ft;
    uint64_t tmpres = 0;
    static int tzflag = 0;

    if (tv)
    {
        GetSystemTimeAsFileTime(&ft);

        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS 11644473600000000ULL
#endif

        /*converting file time to unix epoch*/
        tmpres /= 10;  /*convert into microseconds*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    return 0;
}

#else
#include <sys/time.h>
#endif

#define BUFF_SIZE       10240

static char             LEVEL[] = "DIWE";
static FILE             *g_log = NULL;
static log_info         g_info = {0};
static pthread_mutex_t  g_mutex;

/**
 *\brief      设置日志文件名
 *\param[in]  time_t timestamp  时间戳
 *\param[out] char *filename    文件名
 *\param[in]  int max           文件名最长
 *\return     无
 */
void xt_log_set_filename(time_t timestamp, char *filename, int max)
{
    struct tm tm;
    localtime_s(&tm, &timestamp);

    if (g_info.cycle == LOG_CYCLE_HOUR)
    {
        snprintf(filename, max, "%s.%d%02d%02d-%02d", g_info.name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour);
    }
    else
    {
        snprintf(filename, max, "%s.%d%02d%02d", g_info.name, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
}

/**
 *\brief      加载配置文件
 *\param[in]  const char *path 路径
 *\param[in]  void *json json根
 *\param[out] p_log_info info    文件名
 *\return     int 0-成功,~0-失败
 */
int xt_log_config(const char *path, void *json, p_log_info info)
{
    cJSON *root = (cJSON*)json;

    if (NULL == root)
    {
        printf("%s param is null\n", __FUNCTION__);
        return -1;
    }

    cJSON *log = cJSON_GetObjectItem(root, "log");

    if (NULL == log)
    {
        printf("%s config no log node\n", __FUNCTION__);
        return -2;
    }

    cJSON *file = cJSON_GetObjectItem(log, "file");

    if (NULL == file)
    {
        printf("%s config no log.file node\n", __FUNCTION__);
        return -3;
    }

    snprintf(info->name, sizeof(info->name), "%s%s", path, file->valuestring);

    cJSON *level = cJSON_GetObjectItem(log, "level");

    if (NULL == level)
    {
        printf("%s config no log.level node\n", __FUNCTION__);
        return -3;
    }

    if (0 == strcmp(level->valuestring, "debug"))
    {
        info->level = LOG_LEVEL_DEBUG;
    }
    else if (0 == strcmp(level->valuestring, "info"))
    {
        info->level = LOG_LEVEL_INFO;
    }
    else if (0 == strcmp(level->valuestring, "warn"))
    {
        info->level = LOG_LEVEL_WARN;
    }
    else if (0 == strcmp(level->valuestring, "error"))
    {
        info->level = LOG_LEVEL_ERROR;
    }
    else
    {
        printf("%s config no log.level value error\n", __FUNCTION__);
        return -4;
    }

    cJSON *cycle = cJSON_GetObjectItem(log, "cycle");

    if (NULL == cycle)
    {
        printf("%s config no log.cycle node\n", __FUNCTION__);
        return -5;
    }

    if (0 == strcmp(cycle->valuestring, "hour"))
    {
        info->cycle = LOG_CYCLE_HOUR;
    }
    else if (0 == strcmp(cycle->valuestring, "day"))
    {
        info->cycle = LOG_CYCLE_DAY;
    }
    else if (0 == strcmp(cycle->valuestring, "week"))
    {
        info->cycle = LOG_CYCLE_WEEK;
    }
    else
    {
        printf("%s config no log.cycle value error\n", __FUNCTION__);
        return -6;
    }

    cJSON *backup = cJSON_GetObjectItem(log, "backup");

    if (NULL == backup)
    {
        printf("%s config no log.backup value error\n", __FUNCTION__);
        return -7;
    }

    info->backup = backup->valueint;

    return 0;
}


/**
 *\brief      初始化日志
 *\param[in]  p_log_info info   信息
 *\return     int 0-成功,~0-失败
 */
int xt_log_init(p_log_info info)
{
    if (NULL == info ||
        NULL == info->name ||
        info->level < 0 ||
        info->level > LOG_LEVEL_ERROR ||
        info->cycle < 0 ||
        info->cycle > LOG_CYCLE_WEEK ||
        info->backup < 0)
    {
        return -1;
    }

    if (NULL != g_log)
    {
        xt_log_uninit();
    }

    pthread_mutex_init(&g_mutex, NULL);
    pthread_mutex_lock(&g_mutex);

    g_info = *info;
    g_info.last_reopen_time = 0;

    char filename[512] = "";
    xt_log_set_filename(time(NULL), filename, sizeof(filename));
    fopen_s(&g_log, filename, "ab+");

    pthread_mutex_unlock(&g_mutex);

    return (NULL == g_log);
}

/**
 *\brief      反初始化日志
 *\return     无
 */
void xt_log_uninit()
{
    if (NULL != g_log)
    {
        fflush(g_log);
        fclose(g_log);
        g_log = NULL;
    }

    pthread_mutex_destroy(&g_mutex);
}

/**
 *\brief      刷新日志
 *\return     int 是否更新文件
 */
int xt_log_schedule()
{
    if (0 == g_info.backup)
    {
        return 0;
    }

    int reopen_time = 0;
    int timestamp = (int)time(NULL);

    switch (g_info.cycle)
    {
        case LOG_CYCLE_HOUR:
        {
            reopen_time = (timestamp + 28800) / 3600;
            break;
        }
        case LOG_CYCLE_DAY:
        {
            reopen_time = (timestamp + 28800) / 86400;
            break;
        }
        case LOG_CYCLE_WEEK:
        {
            reopen_time = (timestamp + 28800) / 604800;
            break;
        }
    }

    if (0 == g_info.last_reopen_time)
    {
        g_info.last_reopen_time = reopen_time;
    }

    if (reopen_time != g_info.last_reopen_time)
    {
        g_info.last_reopen_time = reopen_time;

        switch (g_info.cycle)
        {
            case LOG_CYCLE_HOUR:
            {
                timestamp -= g_info.backup * 3600;
                break;
            }
            case LOG_CYCLE_DAY:
            {
                timestamp -= g_info.backup * 86400;
                break;
            }
            case LOG_CYCLE_WEEK:
            {
                timestamp -= g_info.backup * 604800;
                break;
            }
        }

        char filename[256] = "";
        xt_log_set_filename(timestamp, filename, sizeof(filename));
        _unlink(filename); // 删除旧文件
        DBG("unlink %s", filename);

        return true;
    }

    return false;
}

/**
 *\brief      新建日志
 *\return     无
 */
void xt_log_reopen()
{
    char filename[256];
    xt_log_set_filename(time(NULL), filename, sizeof(filename));
    DBG("freopening %s", filename);
    freopen_s(&g_log, filename, "ab+", stderr);
    DBG("freopened %s", filename);
}

/**
 *\brief      反初始化日志
 *\return     无
 *\param[in]  int pid           进程ID
 *\param[in]  const char *file  文件名
 *\param[in]  const char *func  函数名
 *\param[in]  int line          行号
 *\param[in]  int level         日志级别
 *\param[in]  const char *fmt   日志内容
 */
void xt_log_write(int pid, const char *file, const char *func, int line, int level, const char *fmt, ...)
{
    if (pid < 0 ||
        level < LOG_LEVEL_DEBUG ||
        level > LOG_LEVEL_ERROR ||
        level < g_info.level ||
        NULL == fmt ||
        NULL == g_log)
    {
        return;
    }

    int len;
    char buff[BUFF_SIZE];
    time_t sec;
    va_list arg;
    struct tm tm;
    struct timeval tv;


    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    localtime_s(&tm, &sec);

    len = snprintf(buff, BUFF_SIZE, "%02d:%02d:%02d.%03d|%d|%c|%s:%d|%s|", 
                   tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000, pid, LEVEL[level], file, line, func);

    va_start(arg, fmt);
    len += vsnprintf(&buff[len], BUFF_SIZE - len, fmt, arg);
    va_end(arg);

    if (len >= BUFF_SIZE)
    {
        len = (int)strlen(buff); // 当buff不够时,vsnprintf返回的是需要的长度
    }

    buff[len] = '\n';

    fwrite(buff, 1, len + 1, g_log); // 加1为多了个\n
    fflush(g_log);
}

void xt_log(const char *fmt, ...)
{
    int len;
    char buff[BUFF_SIZE];
    time_t sec;
    va_list arg;
    struct tm tm;
    struct timeval tv;

    if (NULL == g_log || NULL == fmt)
    {
        return;
    }

    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    localtime_s(&tm, &sec);

    len = snprintf(buff, BUFF_SIZE, "%02d:%02d:%02d.%03d|", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);

    va_start(arg, fmt);
    len += vsnprintf(&buff[len], BUFF_SIZE - len, fmt, arg);
    va_end(arg);

    if (len >= BUFF_SIZE)
    {
        len = (int)strlen(buff);
    }

    buff[len] = '\n';

    fwrite(buff, 1, len + 1, g_log);
    fflush(g_log);
}
