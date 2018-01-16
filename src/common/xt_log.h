/*************************************************
 * XT
 * Copyright (C) 2013-2015 XT. Co., Ltd.
 * File name:      xt_log.h
 * Author:         张海涛
 * Version:        1.0.0
 * Date:           2016.12.07
 * Description:    日志定义
*************************************************/

#ifndef _XT_LOG_H_
#define _XT_LOG_H_


enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_CYCLE_HOUR = 0,
    LOG_CYCLE_DAY,
    LOG_CYCLE_WEEK,
};

typedef struct _log_info
{
    char name[512];
    int level;
    int cycle;
    int backup;
    int last_reopen_time;

}log_info, *p_log_info;

#ifdef WINDOWS
#include <windows.h>
#include <process.h>
#define close           closesocket
#define sleep(n)        Sleep(n*1000)
#define strncasecmp     strnicmp
#define PATH_SEGM       '\\'
#define DBG(...) xt_log_write(_getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_DEBUG, __VA_ARGS__) // 当直接输出含有%的字符串时,会出异常
#define MSG(...) xt_log_write(_getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_INFO, __VA_ARGS__)
#define WAR(...) xt_log_write(_getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_WARN, __VA_ARGS__)
#define ERR(...) xt_log_write(_getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define PATH_SEGM       '/'
#define DBG(args...) xt_log_write(getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_DEBUG, ##args)
#define MSG(args...) xt_log_write(getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_INFO, ##args)
#define WAR(args...) xt_log_write(getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_WARN, ##args)
#define ERR(args...) xt_log_write(getpid(), __FILE__, __FUNCTION__, __LINE__, LOG_LEVEL_ERROR, ##args)
#endif

#ifndef bool
#define bool  unsigned char
#endif

#ifndef true
#define true  1
#endif

#ifndef false
#define false 0
#endif

#ifndef NULL
#define NULL 0
#endif


/**
 *\brief      加载配置文件
 *\param[in]  const char *path 路径
 *\param[in]  void *json json根
 *\param[out] p_log_info info    文件名
 *\return     int 0-成功,~0-失败
 */
int xt_log_config(const char *path, void *json, p_log_info info);

/**
 *\brief      初始化日志
 *\param[in]  p_log_info info   信息
 *\return     int 0-成功,~0-失败
 */
int xt_log_init(p_log_info info);

/**
 *\brief      反初始化日志
 *\return     无
 */
void xt_log_uninit();

/**
 *\brief      刷新日志
 *\return     int 是否更新文件
 */
int xt_log_schedule();

/**
 *\brief      新建日志
 *\return     无
 */
void xt_log_reopen();

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
void xt_log_write(int pid, const char *file, const char *func, int line, int level, const char *fmt, ...);
void xt_log(const char *fmt, ...);
#endif
