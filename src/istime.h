//
// Created by suitm on 2020/12/26.
//

#ifndef ISTIME_H
#define ISTIME_H

#define ISTIME_VERSION 20201226

/** 取当前时间的毫秒数 **/
uint64_t istime_us();

/** 取当时时间的秒数 **/
uint64_t istime_s();

/**  取当前时间的纳秒数，注意该程序500年后会失效，因为会超过uint64的最大长度 **/
uint64_t istime_ns();

/** 获取当日的时间信息，6位长 **/
uint64_t istime_time();

/** 获取当日的日期，8位长 **/
uint64_t istime_date();

/** 获取当日的日期+时间， 14位长 **/
uint64_t istime_datetime();

#endif //ISTIME_H
