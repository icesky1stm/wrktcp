//
// Created by suitm on 2020/12/26.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/types.h>
struct timespec ts;

#include "istime.h"

/*******
uint32_t的长度(10-1位): 4294967296----------
uint64_t的长度(20-1位): 18446744073709551615
 */

uint64_t istime_us(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000000) + t.tv_usec;
}

uint64_t istime_s(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec;
}

/** 约500多年后，该程序不可用 **/
uint64_t istime_ns(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec*1000000000 + ts.tv_nsec;
}

uint64_t istime_time(){
    time_t t = time(0);
    struct tm local_time;

    localtime_r(&t, &local_time);
    return local_time.tm_hour*10000 + local_time.tm_min*100 + local_time.tm_sec;
}

uint64_t istime_date(){
    time_t t = time(0);
    struct tm local_time;

    localtime_r(&t, &local_time);
    return (local_time.tm_year+1900)*10000 + (local_time.tm_mon+1)*100 + local_time.tm_mday;
}
uint64_t istime_datetime(){
    time_t t = time(0);
    struct tm local_time;

    /* localtime_r 是线程安全的 */
    localtime_r(&t, &local_time);
    return (local_time.tm_year+1900)*10000000000 + (local_time.tm_mon+1)*100000000 + local_time.tm_mday*1000000
    + local_time.tm_hour*10000 + local_time.tm_min*100 + local_time.tm_sec;
}
