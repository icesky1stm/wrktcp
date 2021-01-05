//
// Created by suitm on 2020/12/15.
//

#ifndef ISLOG_H
#define ISLOG_H

#define ISLOG_VERSION_NO "20.12.1"
char * islog_version();

void islog_print(char *file, long line, char *level, char *fmtstr, ...);

/** 通过宏的方式控制日志是否输出，
 * 因为在预编译时执行,对效率完全没有影响
 * 但是修改需要重新编译引用的头文件 **/
#define ISLOG_ERROR_ENABLE
#define ISLOG_WARN_ENABLE
#define ISLOG_INFO_ENABLE
#define ISLOG_DEBUG_ENABLE_NO

/** 定义各种类型的日志输出 **/
#ifdef ISLOG_ERROR_ENABLE
#define islog_error(...)  \
do{                      \
    islog_print(__FILE__,__LINE__, "error", __VA_ARGS__);\
}while(0)
#else
#define islog_error(...)
#endif

#ifdef ISLOG_WARN_ENABLE
 #define islog_warn(...)    islog_print(__FILE__,__LINE__, "warn", __VA_ARGS__)
#else
 #define islog_warn(...)
#endif

#ifdef ISLOG_INFO_ENABLE
 #define islog_info(...)    islog_print(__FILE__,__LINE__, "info", __VA_ARGS__)
#else
 #define islog_info(...)
#endif

#ifdef ISLOG_DEBUG_ENABLE
 #define islog_debug(...)    islog_print(__FILE__,__LINE__, "debug", __VA_ARGS__)
#else
 #define islog_debug(...)
#endif


#endif //ISLOG_H
