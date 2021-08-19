//
// Created by suitm on 2020/12/26.
//

#ifndef ISTIME_H
#define ISTIME_H
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

#define ISTIME_VERSION_NO "21.08.2"
char * istime_version();

/** 取当前时间的毫秒数 **/
uint64_t istime_us();

/** 根据time_us微秒信息，生成ISO8601的标准日期时间格式, 其实秒以下是舍弃的 **/
char * istime_iso8601(char * strtime, uint32_t maxsize, uint64_t time_us);

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
uint32_t istime_strftime(char * strtime, uint32_t maxsize, const char * format, uint64_t time_us);

/** 获取当日的时间信息，6位长 **/
long istime_longtime();

/** 获取当日的日期，8位长 **/
long istime_longdate();

/** 获取当前时区的信息,返回秒东八区是-28800(-8*3600) **/
unsigned long istime_timezone();

#endif //ISTIME_H
