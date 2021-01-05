//
// Created by suitm on 2020/12/26.
//
/**
uint32_t的长度(10-1位): 4294967296----------
uint64_t的长度(20-1位): 18446744073709551615
**/

#include <time.h>
#include <sys/time.h>

#include "istime.h"
static void nolocks_localtime(time_t *p_t, struct tm *tmp);

char * istime_version(){
    return ISTIME_VERSION_NO;
}

/** 获取当前时间，到微秒 **/
uint64_t istime_us(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000000) + t.tv_usec;
}

/** 使用time_us作为输出，调用strftime **/

/*
 * format格式内容
%a 星期几的简写 %A 星期几的全称 %b 月份的简写 %B 月份的全称 %c 标准的日期的时间串 %C 年份的后两位数字
%d 十进制表示的每月的第几天 %D 月/天/年 %e 在两字符域中，十进制表示的每月的第几天 %F 年-月-日
%g 年份的后两位数字，使用基于周的年 %G 年份，使用基于周的年 %h 简写的月份名 %H 24小时制的小时
%I 12小时制的小时 %j 十进制表示的每年的第几天 %m 十进制表示的月份 %M 十时制表示的分钟数
%n 新行符 %p 本地的AM或PM的等价显示 %r 12小时的时间 %R 显示小时和分钟：hh:mm
%S 十进制的秒数 %t 水平 制表符 %T 显示时分秒：hh:mm:ss %u 每周的第几天，星期一为第一天 （值从1到7，星期一为1）
%U 第年的第几周，把星期日作为第一天（值从0到53） %V 每年的第几周，使用基于周的年 %w 十进制表示的星期几（值从0到6，星期天为0）
%W 每年的第几周，把星期一做为第一天（值从0到53） %x 标准的日期串 %X 标准的时间串 %y 不带世纪的十进制年份（值从0到99）
%Y 带世纪部分的十制年份 %z，%Z 时区名称，如果不能得到时区名称则返回空字符。 %% 百分号

 */
uint32_t istime_strftime(char * strtime, uint32_t maxsize, const char * format, uint64_t time_us){
    struct timeval t;
    t.tv_sec = time_us / 1000000;
    //t.tv_usec = time_us % 1000000;

    struct tm local_time;

    /*** TODO 可能会锁，后续改为redis的 nonblock_localtime 写法 ***/
    nolocks_localtime( &t.tv_sec, &local_time);
    return strftime(strtime, maxsize, format, &local_time);
}

/** iso8601格式的 **/
char * istime_iso8601(char * strtime, uint32_t maxsize, uint64_t time_us){
    char format[]="%Y-%m-%dT%H:%M:%S";
    // 至少 19 位;
    // 2020-12-31T12:12:12
    istime_strftime( strtime, maxsize, format, time_us);
    return strtime;
}

long istime_longtime(){
    time_t t = time(0);
    struct tm local_time;

    nolocks_localtime(&t, &local_time);
    return local_time.tm_hour*10000 + local_time.tm_min*100 + local_time.tm_sec;
}

long istime_longdate(){
    time_t t = time(0);
    struct tm local_time;

    nolocks_localtime(&t, &local_time);
    return (local_time.tm_year+1900)*10000 + (local_time.tm_mon+1)*100 + local_time.tm_mday;
}

/** redis 重写的不会导致死锁的localtime **/
static int is_leap_year(time_t year) {
    if (year % 4) return 0;         /* A year not divisible by 4 is not leap. */
    else if (year % 100) return 1;  /* If div by 4 and not 100 is surely leap. */
    else if (year % 400) return 0;  /* If div by 100 *and* not by 400 is not leap. */
    else return 1;                  /* If div by 100 and 400 is leap. */
}

static void nolocks_localtime(time_t *p_t, struct tm *tmp) {
    /*** 在redis代码的基础上，做了修改  ***/
    static int static_tz_set = 0;
    int dst = 0; // 不支持夏令时，因为这是中国
    if( static_tz_set == 0){
        tzset();
        static_tz_set = 1;
    }

    time_t t = *p_t;
    const time_t secs_min = 60;
    const time_t secs_hour = 3600;
    const time_t secs_day = 3600*24;

    t -= timezone;                      /* Adjust for timezone. */
    t += 3600*dst;                      /* Adjust for daylight time. */
    time_t days = t / secs_day;         /* Days passed since epoch. */
    time_t seconds = t % secs_day;      /* Remaining seconds. */

    tmp->tm_isdst = dst;
    tmp->tm_hour = seconds / secs_hour;
    tmp->tm_min = (seconds % secs_hour) / secs_min;
    tmp->tm_sec = (seconds % secs_hour) % secs_min;

    /* 1/1/1970 was a Thursday, that is, day 4 from the POV of the tm structure
     * where sunday = 0, so to calculate the day of the week we have to add 4
     * and take the modulo by 7. */
    tmp->tm_wday = (days+4)%7;

    /* Calculate the current year. */
    tmp->tm_year = 1970;
    while(1) {
        /* Leap years have one day more. */
        time_t days_this_year = 365 + is_leap_year(tmp->tm_year);
        if (days_this_year > days) break;
        days -= days_this_year;
        tmp->tm_year++;
    }
    tmp->tm_yday = days;  /* Number of day of the current year. */

    /* We need to calculate in which month and day of the month we are. To do
     * so we need to skip days according to how many days there are in each
     * month, and adjust for the leap year that has one more day in February. */
    int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    mdays[1] += is_leap_year(tmp->tm_year);

    tmp->tm_mon = 0;
    while(days >= mdays[tmp->tm_mon]) {
        days -= mdays[tmp->tm_mon];
        tmp->tm_mon++;
    }

    tmp->tm_mday = days+1;  /* Add 1 since our 'days' is zero-based. */
    tmp->tm_year -= 1900;   /* Surprisingly tm_year is year-1900. */
}
