// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include "wrktcp.h"
#include "main.h"

#include "version.h"
#include "islog.h"
#include "net.h"
#include <arpa/inet.h>


#define  TCPINIFILE_MAX_LENGTH 256

static struct config {
    uint64_t connections;
    uint64_t duration;
    uint64_t threads;
    uint64_t timeout;
    uint64_t pipeline;
    uint64_t delay;
    uint64_t realtime;
    uint64_t dynamic;
    uint64_t latency;
    char    *host;
    int     port;
    char    tcpinifile[TCPINIFILE_MAX_LENGTH];
    tcpini  tcpini;
} cfg;

static struct {
    stats *latency;
    stats *requests;
} statistics;

static struct sock sock = {
    .connect  = sock_connect,
    .close    = sock_close,
    .read     = sock_read,
    .write    = sock_write,
    .readable = sock_readable
};

static volatile sig_atomic_t stop = 0;

static void handler(int sig) {
    stop = 1;
}

static void usage() {
    printf("Usage: wrktcp <options> filename                      \n"
           "  Necessary Options:                                  \n"
           "    -c, --connections <N>  Connections to keep open   \n"
           "    -d, --duration    <T>  Duration of test           \n"
           "    -t, --threads     <N>  Number of threads to use   \n"

           "  Condition Options:                                  \n"
           "        --latency          Print latency statistics   \n"
           "        --timeout     <T>  Socket/request timeout     \n"
           "        --realtime    <T>  Print real-time tps/latency\n"
           "                           and output a html chart    \n"

           "    -v, --version          Print version details      \n"
           "                                                      \n"
           "  Numeric arguments may include a SI unit (1k, 1M, 1G)\n"
           "  Time arguments may include a time unit (2s, 2m, 2h) \n"
           "  for example:                                        \n"
           "    wrktcp -t4 -c200 -d30s sample_tiny.ini            \n"
           );
}

int main(int argc, char **argv) {

    /** 解析入口参数 **/
    if (parse_args(&cfg, argc, argv)) {
        islog_error("argv is errror!!!");
        usage();
        exit(1);
    }

    /** 设置信号忽略 **/
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  SIG_IGN);

    /** 设置统计信息初始值 **/
    statistics.latency  = stats_alloc(cfg.timeout * 1000);
    statistics.requests = stats_alloc(MAX_THREAD_RATE_S);
    thread *threads     = zcalloc(cfg.threads * sizeof(thread));

    /** 解析tcpinifile文件 **/
    if(tcpini_file_load(cfg.tcpinifile, &cfg.tcpini) != 0){
        fprintf(stderr, "unable to load tcpinifile %s\n",cfg.tcpinifile);
        exit(1);
    }

    cfg.host = cfg.tcpini.host;
    cfg.port = cfg.tcpini.port;

    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t      = &threads[i];
        t->tno = i + 1;
        t->loop        = aeCreateEventLoop(10 + cfg.connections * 3);
        t->connections = cfg.connections / cfg.threads;

        t->tcpini = &cfg.tcpini;
        if (i == 0) {
            /* 管道发送,目前一次连接一次通讯 */
            cfg.pipeline = 1;
            /* 动态模板 */
            if(cfg.tcpini.paras->value != NULL){
                cfg.dynamic = true;
            }
            /* 延迟发送，暂不支持 */
            cfg.delay = 0;
        }

        if (!t->loop || pthread_create(&t->thread, NULL, &thread_main, t)) {
            char *msg = strerror(errno);
            fprintf(stderr, "unable to create thread %"PRIu64": %s\n", i, msg);
            exit(2);
        }
    }

    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    char *time = format_time_s(cfg.duration);
    printf("Running %s test @ %s:%d using %s\n", time, cfg.host, cfg.port, cfg.tcpinifile);
    printf("  %"PRIu64" threads and %"PRIu64" connections\n", cfg.threads, cfg.connections);

    uint64_t start    = time_us();
    uint64_t complete = 0;
    uint64_t bytes    = 0;
    errors errors     = { 0 };

    /** 如果未设置realtime, 则不刷新和记录 **/
    if( cfg.realtime == 0){
        sleep(cfg.duration);
    }else{
        print_running( statistics.requests, cfg.duration, cfg.realtime);
    }
    stop = 1;
    /** sleep 100ms **/
    usleep(100000);

    /*** 开始统计结果信息 ***/
    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t = &threads[i];
        pthread_join(t->thread, NULL);

        complete += t->complete;
        bytes    += t->bytes;

        errors.connect += t->errors.connect;
        errors.read    += t->errors.read;
        errors.write   += t->errors.write;
        errors.timeout += t->errors.timeout;
        errors.status  += t->errors.status;
    }

    uint64_t runtime_us = time_us() - start;
    long double runtime_s   = runtime_us / 1000000.0;
    long double req_per_s   = complete   / runtime_s;
    long double req_success_per_s = (complete-errors.connect-errors.read-errors.write-errors.timeout-errors.status) / runtime_s;
    long double req_fail_per_s = errors.status / runtime_s;
    long double bytes_per_s = bytes      / runtime_s;

    if (complete / cfg.connections > 0) {
        int64_t interval = runtime_us / (complete / cfg.connections);
        stats_correct(statistics.latency, interval);
    }

    print_stats_header();
    /** 缩短了10倍  **/
    print_stats("Latency", statistics.latency, format_time_us);
    print_stats("Req/Sec", statistics.requests, format_metric10);
    if (cfg.latency) print_stats_latency(statistics.latency);

    char *runtime_msg = format_time_us(runtime_us);

    printf("  %"PRIu64" requests in %s, %sB read\n", complete, runtime_msg, format_binary(bytes));
    if (errors.connect || errors.read || errors.write || errors.timeout) {
        printf("  Socket errors: connect %d, read %d, write %d, timeout %d\n",
               errors.connect, errors.read, errors.write, errors.timeout);
    }

    if (errors.status) {
        printf("  Failure responses: %d\n", errors.status);
    }

    printf("Requests/sec: %9.2Lf\n", req_per_s);
    /** 新增失败的统计 **/
    printf("Requests/sec-Success: %9.2Lf\n", req_success_per_s);
    printf("Requests/sec-Failure: %9.2Lf\n", req_fail_per_s);
    printf("Transfer/sec: %10sB\n", format_binary(bytes_per_s));

    return 0;
}

void *thread_main(void *arg) {
    thread *thread = arg;

    /** 如果不是动态模板，则只初始化一次发送信息,提高效率 **/
    char *request = NULL;
    long length = 0;
    if ( !cfg.dynamic ) {
        if(tcpini_request_parser(thread->tcpini, &request, &length) != 0){
            fprintf(stderr, "parser ini content error !!!");
            return NULL;
        }
    }

    thread->cs = zcalloc(thread->connections * sizeof(connection));
    connection *c = thread->cs;

    for (uint64_t i = 0; i < thread->connections; i++, c++) {
        c->thread = thread;
        c->cno = thread->tno * 10000 + i + 1;
        c->request = request;
        c->length  = length;
        c->delayed = cfg.delay;
        c->tcpini = thread->tcpini;
        islog_debug("c->no---[%ld]", c->cno);
        connect_socket(thread, c);
    }

    aeEventLoop *loop = thread->loop;
    aeCreateTimeEvent(loop, RECORD_INTERVAL_MS, record_rate, thread, NULL);

    thread->start = time_us();
    aeMain(loop);

    aeDeleteEventLoop(loop);
    zfree(thread->cs);

    return NULL;
}

static int connect_socket(thread *thread, connection *c) {
    islog_debug("CONNECT cno:%ld", c->cno);
    struct sockaddr_in sin ;

    memset(&sin, 0x00, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(c->tcpini->port);
    sin.sin_addr.s_addr = inet_addr(c->tcpini->host);
    memset(&(sin.sin_zero),0x00, 8);

    struct aeEventLoop *loop = thread->loop;
    int fd, flags;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (connect(fd, ( struct sockaddr *) & sin, sizeof(sin)) == -1) {
        if (errno != EINPROGRESS) goto error;
    }

    flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

    flags = AE_READABLE | AE_WRITABLE;
    if (aeCreateFileEvent(loop, fd, flags, socket_connected, c) == AE_OK) {
//        c->parser.data = c;
        c->fd = fd;
        return fd;
    }

  error:
    thread->errors.connect++;
    close(fd);
    return -1;
}

static int reconnect_socket(thread *thread, connection *c) {
    islog_debug("RECONNECT cno:%ld", c->cno);
    aeDeleteFileEvent(thread->loop, c->fd, AE_WRITABLE | AE_READABLE);
    sock.close(c);
    close(c->fd);
    int ret = 0;
    if( stop != 1) {
        ret = connect_socket(thread, c);
    }
    return ret;
}

static int record_rate(aeEventLoop *loop, long long id, void *data) {
    thread *thread = data;

    if (thread->requests > 0) {
        /** 此处修改了统计方式，wrk的方式是按TPS为整数统计，最大是1000Wtps，
         * 这样会产生两个问题：
         * 1.10000000 * uint64 = 80M的内存占用
         * 2.如果TPS较低，比如响应时间在100ms以上的系统，基本上TPS统计非常的不准确,因为刷新间隔是100MS，导致大量TPS分布都是0
         * 本次为了不增加内存消耗，依旧使用1000W个分布，但是缩减了最大TPS的支持为100WTPS
         * **/

        uint64_t elapsed_ms = (time_us() - thread->start) / 1000;
        uint64_t requests = (thread->requests * 10 / (double) elapsed_ms) * 1000;

        stats_record(statistics.requests, requests);

        thread->requests = 0;
        thread->start    = time_us();
    }

    if (stop) aeStop(loop);

    return RECORD_INTERVAL_MS;
}

static int delay_request(aeEventLoop *loop, long long id, void *data) {
    connection *c = data;
    c->delayed = false;
    aeCreateFileEvent(loop, c->fd, AE_WRITABLE, socket_writeable, c);
    return AE_NOMORE;
}

static void socket_connected(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    islog_debug("ALREADY CONNECTED cno:%ld", c->cno);

    switch (sock.connect(c, cfg.host)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }

    /** 初始化接收的信息 **/
    if(tcpini_response_init(c) != 0){
        islog_error(" response init error");
        goto error;
    }
    c->written = 0;

    aeCreateFileEvent(c->thread->loop, fd, AE_READABLE, socket_readable, c);
    aeCreateFileEvent(c->thread->loop, fd, AE_WRITABLE, socket_writeable, c);

    return;

  error:
    c->thread->errors.connect++;
    reconnect_socket(c->thread, c);
}

static void socket_writeable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    thread *thread = c->thread;
    islog_debug("WRITEABLE cno:%ld", c->cno);

    if (c->delayed) {
//        uint64_t delay = script_delay(thread->L);
        uint64_t delay = 1000;
        aeDeleteFileEvent(loop, fd, AE_WRITABLE);
        aeCreateTimeEvent(loop, delay, delay_request, c, NULL);
        return;
    }

    /** 等于0的时候，就是要发送的时候 **/
    if (!c->written) {
        /*** 如果是动态模板,则每次写的时候都初始化,效率不高 ***/
        if (cfg.dynamic) {
            if(tcpini_request_parser( &cfg.tcpini, &c->request, &c->length) != 0){
                fprintf(stderr, "writer parser ini content error !!!");
                goto error;
            }
        }
        c->start   = time_us();
        c->pending = cfg.pipeline;
    }

    char  *buf = c->request + c->written;
    size_t len = c->length  - c->written;
    size_t n;

    switch (sock.write(c, buf, len, &n)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }

    c->written += n;
    islog_debug("send msg [%s][%d]", buf, c->written);
    /** 如果发送到了长度，则删除时间，等读取完成再注册 **/
    if (c->written == c->length) {
        c->written = 0;
        aeDeleteFileEvent(loop, fd, AE_WRITABLE);
    }

    return;

  error:
    thread->errors.write++;
    reconnect_socket(thread, c);
}

static void socket_readable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    islog_debug("READABLE cno:%ld", c->cno);
    size_t n;

    do {
        switch (sock.read(c, &n)) {
            case OK:    break;
            case ERROR: goto error;
            case RETRY: return;
        }
        islog_debug("read buf is [%s]", c->buf);

        if(tcpini_response_parser(c, c->buf, n) != 0){
            islog_error("tcpini_response_parser error");
            goto error;
        }

        /** 如果报文处理完成了，则进行统计等处理 **/
        if( response_complete( c, c->buf, n) != 0){
            islog_error("response_complete error");
            goto error;
        }

//        if (http_parser_execute(&c->parser, &parser_settings, c->buf, n) != n) goto error;
//        if (n == 0 && !http_body_is_final(&c->parser)) goto error;

        c->thread->bytes += n;
    } while (n == RECVBUF && sock.readable(c) > 0);

    return;

  error:
    c->thread->errors.read++;
    reconnect_socket(c->thread, c);
}

static uint64_t time_us() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000000) + t.tv_usec;
}

static struct option longopts[] = {
    { "connections", required_argument, NULL, 'c' },
    { "duration",    required_argument, NULL, 'd' },
    { "threads",     required_argument, NULL, 't' },
    { "latency",     no_argument,       NULL, 'L' },
    { "timeout",     required_argument, NULL, 'T' },
    { "realtime",    required_argument, NULL, 'R' },
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { NULL,          0,                 NULL,  0  }
};

static int parse_args(struct config *cfg, int argc, char **argv) {
    int c;

    memset(cfg, 0, sizeof(struct config));
    cfg->threads     = 2;
    cfg->connections = 10;
    cfg->duration    = 10;
    cfg->timeout     = SOCKET_TIMEOUT_MS;

    while ((c = getopt_long(argc, argv, "t:c:d:H:T:Lrv?", longopts, NULL)) != -1) {
        switch (c) {
            case 't':
                if (scan_metric(optarg, &cfg->threads)) return -1;
                break;
            case 'c':
                if (scan_metric(optarg, &cfg->connections)) return -1;
                break;
            case 'd':
                if (scan_time(optarg, &cfg->duration)) return -1;
                break;
            case 'L':
                cfg->latency = true;
                break;
            case 'T':
                if (scan_time(optarg, &cfg->timeout)) return -1;
                cfg->timeout *= 1000;
                break;
            case 'R':
                if (scan_time(optarg, &cfg->realtime)) return -1;
                break;
            case 'v':
                printf("wrktcp version %s [%s] ", WRKVERSION, aeGetApiName());
                printf("Copyright (C) 2020 icesky\n");
                break;
            case 'h':
            case '?':
            case ':':
            default:
                islog_error("command is error, input the illegal character:[%c]", c);
                return -1;
        }
    }

    if (optind == argc || !cfg->threads || !cfg->duration) return -1;

    strcpy( cfg->tcpinifile, argv[optind]);
    /* 检查压测脚本是否符合要求 */
    if(strlen( cfg->tcpinifile) >= TCPINIFILE_MAX_LENGTH || strlen(cfg->tcpinifile) <= 0){
        fprintf(stderr, "filename's length is not allow [%ld]\n", strlen( cfg->tcpinifile));
        return -1;
    }
    if( strstr( cfg->tcpinifile, ".ini") == NULL){
        fprintf( stderr, "tcpini file must be ended with .ini,[%s]", cfg->tcpinifile);
        return -1;
    }

    /* 检查连接和线程数的关系 */
    if (!cfg->connections || cfg->connections < cfg->threads) {
        fprintf(stderr, "number of connections must be >= threads\n");
        return -1;
    }
    /* 检查realtime必须小于duration */
    if( cfg->realtime >= cfg->duration){
        fprintf(stderr, "realtime:%llu must less than duration:%llu \n", cfg->realtime, cfg->duration);
        return -1;
    }

    return 0;
}

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

/* 执行过程中刷新统计结果 */
static void print_running(stats  *stats, uint64_t time, uint64_t realtime){
    uint64_t i ;
    for( i =1; i <= time; i++){
        /** 收到打断信号, 则直接退出 **/
        if( stop == 1 ){
            break;
        }
       sleep(realtime);
        printf("\r");

        uint64_t max = stats->max;
        long double mean  = stats_mean(stats);
        long double stdev = stats_stdev(stats, mean);

        printf("    %-10s", "Running Req/Sec ");
        print_units(mean,  format_metric, 8);
        print_units(stdev, format_metric, 10);
        print_units(max,   format_metric, 9);
        printf("%8.2Lf%%", stats_within_stdev(stats, mean, stdev, 1));

        fflush(stdout);
    }

    return;
}
static int response_complete(void * data, char * buf, size_t n) {
    connection *c = data;
    thread *thread = c->thread;

    if(c->rsp_state != COMPLETE){
        return 0;
    }

    uint64_t now = time_us();

    thread->complete++;
    thread->requests++;

    /** 匹配结果，失败的要单独统计 **/
    islog_debug("rsp_head[%s], rsp_body[%s]", c->rsp_head, c->rsp_body);
    if(tcpini_response_issuccess(c->tcpini, c->rsp_head, c->rsp_body)){
        islog_debug(" response is success !!!!");
        /*EMPTY*/;
    }else{
        islog_debug(" response is failure !!!!");
        thread->errors.status++;
    }

    if (--c->pending == 0) {
        if (!stats_record(statistics.latency, now - c->start)) {
            thread->errors.timeout++;
        }
        c->delayed = cfg.delay;
        aeCreateFileEvent(thread->loop, c->fd, AE_WRITABLE, socket_writeable, c);
    }

    zfree(c->rsp_head);
    zfree(c->rsp_body);
    c->rsp_state = HEAD;
    memset( c->buf, 0x00, sizeof(c->buf));

    /** tcp非长连接,需要重新连接 **/
    reconnect_socket(thread, c);

    return 0;
}