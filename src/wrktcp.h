#ifndef WRKTCP_H
#define WRKTCP_H

#include "config.h"
#include <pthread.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>

#include "stats.h"
#include "ae.h"
#include "tcpini.h"
#include "zmalloc.h"

#define RECVBUF  8192

/** 最大支持的TPS/10, 目前的是 100W **/
#define MAX_THREAD_RATE_S   10000000
/** 超时时间,设置的越大，占用的空间越大 **/
#define SOCKET_TIMEOUT_MS   5000
/** 采样间隔时间 **/
#define RECORD_INTERVAL_MS  100
/** 最大跟踪记录点 **/
#define TRACE_MAX_POINT 100
/** 输出文件最大长度 **/
#define MAX_HTML_FILELEN 512


typedef struct {
    pthread_t thread;
    long tno;
    struct config *lcfg;
    aeEventLoop *loop;
    uint64_t connections;
    uint64_t complete;
    uint64_t requests;
    uint64_t bytes;
    uint64_t start;
    uint64_t latency;
    tcpini *tcpini;
    errors errors;
    struct connection *cs;
} thread;

typedef struct connection {
    struct config *lcfg;
    thread *thread;
    /** 连接数据 **/
    long cno;
    int fd;
    char delayed;
    uint64_t start; /*统计时间，开始*/
    tcpini *tcpini;
    /** 请求数据 **/
    char *request; /*请求整体*/
    long length; /*请求长度*/
    long written; /*已发送长度*/
    uint64_t pending; /*pipeline*/
    /** 应答数据 **/
    char buf[RECVBUF];
    unsigned long readlen;
    enum{
        HEAD, BODY, COMPLETE
    } rsp_state;
    unsigned long rsp_len;
    char * rsp_head;
    char * rsp_body;
} connection;

/** wrk的整体汇总数据 **/
typedef struct config {
    uint64_t connections;   //连接数
    uint64_t duration;      //持续时间-秒
    uint64_t threads;       //线程数
    uint64_t timeout;       //超时时间
    uint64_t pipeline;      //多次发送，暂时没有用
    char isdelay;         //是否 延迟发送
    char isdynamic;       //是否 有动态模板
    char islatency;       //是否 打印响应时间
    char istrace;         //是否 打印trace明细
    char ishtml;          //是否 输出文件
    char htmlfile[MAX_HTML_FILELEN];     //输出html文件名
    thread * p_threads;     //线程的内容
    /** tcp报文的结构配置信息 **/
    tcpini  tcpini;
    /*** 结果记录，汇总的结果数据 ***/
    struct _result{
        int64_t complete;
        int64_t bytes;
        errors errors;
        uint64_t tm_start;
        uint64_t tm_end;
        long double runtime_s;
        uint64_t runtime_us;
        long double req_per_s;
        long double req_success_per_s;
        long double req_fail_per_s;
        long double bytes_per_s;
        long double tps_min;
        long double tps_max;
    }result;
    /** 分布统计,计算平均值方差等 **/
    struct _statistics{
        stats *latency;
        stats *requests;
    } statistics;
    /** 追踪统计，记录时间变化趋势 **/
    struct _trace{
        int64_t step_time;
        int use_num;
        double tps[TRACE_MAX_POINT];
        double latency[TRACE_MAX_POINT];
    }trace;
} config;

#endif /* WRKTCP_H */
