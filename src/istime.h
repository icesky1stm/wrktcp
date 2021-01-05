//
// Created by suitm on 2020/12/26.
//

#ifndef ISTIME_H
#define ISTIME_H
#include <inttypes.h>

#define ISTIME_VERSION_NO "21.01.1"
char * istime_version();

/** 取当前时间的毫秒数 **/
uint64_t istime_us();

/** 根据time_us微秒信息，生成ISO8601的标准日期时间格式, 其实秒以下是舍弃的 **/
char * istime_iso8601(char * strtime, uint32_t maxsize, uint64_t time_us);

uint32_t istime_strftime(char * strtime, uint32_t maxsize, const char * format, uint64_t time_us);

/** 获取当日的时间信息，6位长 **/
long istime_longtime();

/** 获取当日的日期，8位长 **/
long istime_longdate();

#endif //ISTIME_H
