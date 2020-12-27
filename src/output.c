//
// Created by suitm on 2020/12/26.
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

#include "units.h"
#include "output.h"
#include "islog.h"
#include "istime.h"
#include "outputhtml.h"

static void print_stats_header() {
    printf("  Thread Stats%6s%11s%8s%12s\n", "Avg", "Stdev", "Max", "+/- Stdev");
}

static void print_units(long double n, char *(*fmt)(long double), int width) {
    char *msg = fmt(n);
    int len = strlen(msg), pad = 2;

    if (isalpha(msg[len-1])) pad--;
    if (isalpha(msg[len-2])) pad--;
    width -= pad;

    printf("%*.*s%.*s", width, width, msg, pad, "  ");

    free(msg);
}

static void print_stats(char *name, stats *stats, char *(*fmt)(long double)) {
    uint64_t max = stats->max;
    long double mean  = stats_mean(stats);
    long double stdev = stats_stdev(stats, mean);

    printf("    %-10s", name);
    print_units(mean,  fmt, 8);
    print_units(stdev, fmt, 10);
    print_units(max,   fmt, 9);
    printf("%8.2Lf%%\n", stats_within_stdev(stats, mean, stdev, 1));
}

static void print_stats_latency(stats *stats) {
    long double percentiles[] = { 50.0, 75.0, 90.0, 99.0 };
    printf("  Latency Distribution\n");
    for (size_t i = 0; i < sizeof(percentiles) / sizeof(long double); i++) {
        long double p = percentiles[i];
        uint64_t n = stats_percentile(stats, p);
        printf("%7.0Lf%%", p);
        print_units(n, format_time_us, 10);
        printf("\n");
    }
}

/**
 * output_console 输出结果到控制台上
 * @param lcfg
 * @return
 */

int output_console( config * lcfg){
    errors * errors = &lcfg->result.errors;
    int64_t complete = lcfg->result.complete;
    int64_t bytes = lcfg->result.bytes;

    /** 输出分布统计信息头 **/
    print_stats_header();
    /* 缩短了10倍  */
    print_stats("Latency", lcfg->statistics.latency, format_time_us);
    print_stats("Req/Sec", lcfg->statistics.requests, format_metric10);
    if (lcfg->islatency) print_stats_latency(lcfg->statistics.latency);

    char *runtime_msg = format_time_us(lcfg->result.runtime_us);

    printf("  %"PRIu64" requests in %s, %sB read\n", complete, runtime_msg, format_binary(bytes));
    if (errors->connect || errors->read || errors->write || errors->timeout) {
        printf("  Socket errors: connect %d, read %d, write %d, timeout %d\n",
               errors->connect, errors->read, errors->write, errors->timeout);
    }

    if (errors->status) {
        printf("  Failure responses: %d\n", errors->status);
    }

    printf("Requests/sec: %9.2Lf ", lcfg->result.req_per_s);
    /** 新增失败的统计 **/
    printf("   (Success:%.2Lf/", lcfg->result.req_success_per_s);
    printf("Failure:%.2Lf)\n", lcfg->result.req_fail_per_s);
    printf("Transfer/sec: %10sB\n", format_binary(lcfg->result.bytes_per_s));

    /** 新增跟踪趋势统计 **/
    if( lcfg->istrace){
        int i = 0;
        printf("  Trace Details:\n");
        printf("   Time      Tps        Latency\n");
        for( i = 0; i < TRACE_MAX_POINT; i++ ){
            if( lcfg->trace.tps[i] >=  -0.005 && lcfg->trace.tps[i] <= 0.005){
                break;
            }
            printf("  %4"PRIu64"s", i*lcfg->trace.step_time +1);
            printf("  %9.2lf", lcfg->trace.tps[i]);
            printf("  %10s",  format_time_us(lcfg->trace.latency[i]));
            printf("\n");
        }
    }
    return 0;
}

/**
 * output_html 输出到html文件中
 * @param lcfg
 * @return
 */
int output_html( config * lcfg){
    FILE *fp = NULL;
    if( strlen(lcfg->htmlfile) == 0){
        strcpy( lcfg->htmlfile, lcfg->tcpini.file);
        sprintf( strstr( lcfg->htmlfile, ".ini"), ".%"PRIu64".html", istime_datetime());
    }
    printf("Result html :   %s\n",  lcfg->htmlfile);

    fp = fopen( lcfg->htmlfile, "w");
    if( fp == NULL){
        islog_error(" open htmlfile error: %s", strerror(errno));
        return -5;
    }
    fprintf(fp, "%s", g_wrk_output_html_head);
    /***  表头数据  ***/
#if 0
    "        tableData: [\n"
    "          '1',\n"
    "          '136服务器',\n"
    "          '60S',\n"
    "          '100',\n"
    "          '1.245',\n"
    "          '0.875',\n"
    "          '0.763',\n"
    "          '120',\n"
    "          '130',\n"
    "          '150',\n"
    "        ]\n";
#endif
    fprintf(fp,"        tableData: [\n");
    fprintf(fp,"        '%s', \n", "No.1");
    fprintf(fp,"        '%s', \n", lcfg->tcpini.file);
    fprintf(fp,"        '%"PRIu64"s', \n", lcfg->duration);
    fprintf(fp,"        '%"PRIu64"', \n", lcfg->connections);
    /** tps 信息 **/
    fprintf(fp,"        '%.2Lf', \n", lcfg->result.tps_min);
    fprintf(fp,"        '%.2Lf', \n", lcfg->result.req_success_per_s);
    fprintf(fp,"        '%.2Lf', \n", lcfg->result.tps_max);
    /** 响应时间信息 **/
    fprintf(fp,"        '%s', \n", format_time_us(lcfg->statistics.latency->min));
    fprintf(fp,"        '%s', \n", format_time_us(stats_mean(lcfg->statistics.latency)));
    fprintf(fp,"        '%s', \n", format_time_us(lcfg->statistics.latency->max));
    fprintf(fp,"        ],\n");
    /*** 横坐标轴，时间  ***/
#if 0
    "        sendTime: [\n"
"          '00:00:00',\n"
"          '00:00:10',\n"
"          '00:00:20',\n"
"          '00:00:30',\n"
"          '00:00:40',\n"
"          '00:00:50',\n"
"          '00:01:00',\n"
"          '00:01:10',\n"
"          '00:01:20',\n"
"          '00:01:30',\n"
"          '00:01:40',\n"
"          '00:01:50',\n"
"          '00:02:00',\n"
"          '00:02:10',\n"
"          '00:02:20',\n"
"          '00:02:30',\n"
"          '00:02:40',\n"
"          '00:02:50',\n"
"          '00:03:00',\n"
"        ],\n"
#endif
    fprintf(fp,"        sendTime: [\n");
    uint64_t i = 0;
    for ( i = 0; i < 100; i++){
        fprintf(fp,"          '%"PRIu64"s',\n", i*lcfg->trace.step_time + 1);
    }
    fprintf(fp,"        ],\n");
    /** tps数据 **/
#if 0
    "        tpsData: [\n"
"          '700',\n"
"          '800',\n"
"          '850',\n"
"          '900',\n"
"          '1110',\n"
"          '990',\n"
"          '950',\n"
"          '1000',\n"
"          '820',\n"
"          '970',\n"
"          '880',\n"
"          '990',\n"
"          '830',\n"
"          '770',\n"
"          '800',\n"
"          '720',\n"
"          '810',\n"
"          '700',\n"
"          '840',\n"
"        ],\n"
#endif
    fprintf(fp,"        tpsData: [\n");
    for ( i = 0; i < 100; i++){
        fprintf(fp,"          '%.2lf',\n", lcfg->trace.tps[i]);
    }
    fprintf(fp,"        ],\n");
    /** latency 数据 **/
#if 0
    "        resTimeData: [\n"
    "          '0.7',\n"
    "          '0.8',\n"
    "          '0.85',\n"
    "          '0.9',\n"
    "          '0.66',\n"
    "          '0.99',\n"
    "          '0.95',\n"
    "          '0.78',\n"
    "          '0.82',\n"
    "          '0.97',\n"
    "          '0.88',\n"
    "          '0.99',\n"
    "          '0.83',\n"
    "          '0.77',\n"
    "          '0.8',\n"
    "          '0.72',\n"
    "          '0.81',\n"
    "          '0.7',\n"
    "          '0.84',\n"
    "        ],\n"
#endif
    fprintf(fp,"        resTimeData: [\n");
    for ( i = 0; i < 100; i++){
        fprintf(fp,"          '%.2lf',\n", lcfg->trace.latency[i]/(double)1000);
    }
    fprintf(fp,"        ]\n");

    /** 打印结尾 **/
    fprintf(fp, "%s", g_wrk_output_html_tail);
    fclose(fp);
    return 0;
};