// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include "main.h"
#include "wrktcp.h"

/** wrk的整体汇总数据 **/
config cfg;

/** wrk这么写看起来像对象的属性，毫无意义 **/
static struct sock sock = {
        .connect  = sock_connect,
        .close    = sock_close,
        .read     = sock_read,
        .write    = sock_write,
        .readable = sock_readable
};

/**  ctrl +c 打断信号 **/
static volatile sig_atomic_t stop = 0;
static void handler(int sig) {
    stop = 1;
}

/** 使用说明 **/
static void usage() {
    printf("Usage: wrktcp <options> filename                      \n"
           "  Necessary Options:                                  \n"
           "    -c, --connections <N>  Connections to keep open   \n"
           "    -d, --duration    <T>  Duration of test           \n"
           "    -t, --threads     <N>  Number of threads to use   \n"

           "  Condition Options:                                  \n"
           "        --timeout     <T>  Socket/request timeout     \n"
           "        --latency          Print latency statistics   \n"
           "        --trace            Print tps/latency trace    \n"
           "        --html        <T>  output a html chart        \n"
           "        --test             just run once to test ini  \n"

           "    -v, --version          Print version details      \n"
           "                                                      \n"
           "  Numeric arguments may include a SI unit (1k, 1M, 1G)\n"
           "  Time arguments may include a time unit (2s, 2m, 2h) \n"
           "  for example:                                        \n"
           "    wrktcp -t4 -c200 -d30s sample_tiny.ini            \n"
           );
}

/*********************************************************
 * wrktcp 主程序
 * @param argc
 * @param argv
 * @return
 *********************************************************/
int main(int argc, char **argv) {
    /** 设置信号忽略 **/
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  SIG_IGN);

    /** 解析入口参数 **/
    if (parse_args(&cfg, argc, argv)) {
        islog_error("argv is errror!!!");
        usage();
        exit(1);
    }

    /** 设置统计信息初始值 **/
    cfg.statistics.latency  = stats_alloc(cfg.timeout * 1000);
    cfg.statistics.requests = stats_alloc(MAX_THREAD_RATE_S);
    cfg.p_threads = zcalloc(cfg.threads * sizeof(thread));

    /** 解析tcpinifile文件 **/
    if(tcpini_file_load(cfg.tcpini.file, &cfg.tcpini) != 0){
        fprintf(stderr, "unable to load tcpinifile %s\n",cfg.tcpini.file);
        exit(1);
    }

    /** 开始创建线程 **/
    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t      = &cfg.p_threads[i];
        t->tno = i + 1;
        t->loop        = aeCreateEventLoop(10 + cfg.connections * 3);
        t->connections = cfg.connections / cfg.threads;
        t->lcfg = &cfg;
        t->tcpini = &cfg.tcpini;
        if (i == 0) {
            /* 管道发送,目前一次连接一次通讯 */
            cfg.pipeline = 1;
            /* 动态模板 */
            if(cfg.tcpini.paras_pos != 0){
                cfg.isdynamic = 1;
            }
            /* 延迟发送，暂不支持 */
            cfg.isdelay = 0;
        }

        if (!t->loop || pthread_create(&t->thread, NULL, &thread_main, t)) {
            char *msg = strerror(errno);
            fprintf(stderr, "unable to create thread %"PRIu64": %s\n", i, msg);
            exit(2);
        }
    }

    /** 设置ctrl+c打断信号 **/
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    /** 打印输出头 **/
    printf("Running %s loadtest ", format_time_s(cfg.duration));
    printf("@ %s:%d ", cfg.tcpini.host, cfg.tcpini.port);
    printf("using %s", cfg.tcpini.file);
    printf("\n");
    printf("  %"PRIu64" threads and %"PRIu64" connections\n", cfg.threads, cfg.connections);
    if( cfg.istest == 1){
        printf("\n----TEST MODE, connect once and more log----\n");
    }

    /** 初始化基本信息 **/
    cfg.result.tm_start   = istime_us();
    cfg.result.complete = 0;
    cfg.result.bytes = 0;
    memset( &cfg.result.errors, 0x00, sizeof(errors));

    /** 主进程等待后并设置停止标志 **/
    running_sleep( &cfg);
    stop = 1;

    /** 开始记录统计结果信息 **/
    for (uint64_t j = 0; j < cfg.threads; j++) {
        thread *t = &cfg.p_threads[j];
        /* 挨个线程等待 */
        pthread_join(t->thread, NULL);

        cfg.result.complete += t->complete;
        cfg.result.bytes    += t->bytes;

        cfg.result.errors.connect += t->errors.connect;
        cfg.result.errors.read    += t->errors.read;
        cfg.result.errors.write   += t->errors.write;
        cfg.result.errors.timeout += t->errors.timeout;
        cfg.result.errors.status  += t->errors.status;
    }

    cfg.result.tm_end = istime_us();
    cfg.result.runtime_us = cfg.result.tm_end - cfg.result.tm_start;
    cfg.result.runtime_s   = cfg.result.runtime_us / 1000000.0;
    cfg.result.req_per_s   = cfg.result.complete   / cfg.result.runtime_s;
    cfg.result.req_success_per_s = (cfg.result.complete-cfg.result.errors.status) / cfg.result.runtime_s;
    cfg.result.req_fail_per_s = cfg.result.errors.status / cfg.result.runtime_s;
    cfg.result.bytes_per_s = cfg.result.bytes      / cfg.result.runtime_s;

    /** Coordinated Omission 状态修正, 我认为是瞎修正应该使用wrk2的方法，但是为了保持和wrk的结果一致暂时保留 **/
    if (cfg.result.complete / cfg.connections > 0) {
        /* 计算一次连接平均延时多少 */
        int64_t interval = cfg.result.runtime_us / (cfg.result.complete / cfg.connections);
        /* 进行修正，应该学习wrk2，增加-R参数 */
        stats_correct(cfg.statistics.latency, interval);
    }

    if( cfg.istest == 1){
        printf("----TEST MODE, connect once and more log----\n\n");
    }
    /** 输出到屏幕上 **/
    output_console(&cfg);

    /** 输出到html文件中 **/
    if( cfg.ishtml ){
        output_html(&cfg);
    }

    return 0;
}

/** 多线程的主程序，使用epoll创建和管理多个连接 **/
void *thread_main(void *arg) {
    thread *thread = arg;

    /** 如果不是动态模板，则只初始化一次发送信息,提高效率 **/
    char *request = NULL;
    long length = 0;
    if ( !thread->lcfg->isdynamic ) {
        if(tcpini_request_parser(thread->tcpini, &request, &length, NULL) != 0){
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
        c->delayed = thread->lcfg->isdelay;
        c->tcpini = thread->tcpini;
        c->lcfg = thread->lcfg;
        islog_debug("CREATE CONNECTION : cno = %ld", c->cno);
        if( c->tcpini->ishttp){
            /*  http结构体 */
            http_parser_init(&c->http_parser, HTTP_RESPONSE);
            c->http_parser.data = c;
        }

        connect_socket(thread, c);
    }

    aeEventLoop *loop = thread->loop;
    aeCreateTimeEvent(loop, RECORD_INTERVAL_MS, record_rate, thread, NULL);

    thread->start = istime_us();
    aeMain(loop);

    aeDeleteEventLoop(loop);
    zfree(thread->cs);

    return NULL;
}

static int connect_socket(thread *thread, connection *c) {
    islog_debug("CONNECTING cno:%ld", c->cno);
    islog_test("CONNECTING:[%s:%d],cno[%ld]", c->tcpini->host, c->tcpini->port, c->cno);

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
    islog_debug("RECONNECTING cno:%ld", c->cno);
    aeDeleteFileEvent(thread->loop, c->fd, AE_WRITABLE | AE_READABLE);
    sock.close(c);
    close(c->fd);
    int ret = 0;
    if (stop != 1){
        /*** v1.1修改 ***/
        if( c->lcfg->istest == 1) {
            /* 如果是测试场景的话 */
            islog_test("TESTMODE, THEN STOP,cno[%ld]", c->cno);
            stop = 1;
        }else{
            ret = connect_socket(thread, c);
        }
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
         * 这里TPS精度从1->0.1,为了不增加内存消耗，依旧使用1000W个分布，这样相当于缩减了最大TPS的支持到100W TPS
         * **/
        uint64_t elapsed_ms = (istime_us() - thread->start) / 1000;
        uint64_t requests = (thread->requests * 10 / (double) elapsed_ms) * 1000;

        stats_record(cfg.statistics.requests, requests);

        thread->requests = 0;
        thread->start    = istime_us();
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
    islog_test("CONNECTED:[%s:%d],cno[%ld]", c->tcpini->host, c->tcpini->port, c->cno);

    switch (sock.connect(c, c->tcpini->host)) {
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
        if (c->lcfg->isdynamic) {
            if(tcpini_request_parser( c->tcpini, &c->request, &c->length, c) != 0){
                fprintf(stderr, "writer parser ini content error !!!\n");
                exit(1);
//                goto error;
            }
        }
        c->start   = istime_us();
        c->pending = c->lcfg->pipeline;
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
    islog_test("SENDMSG:[%s][%d],cno[%ld]", buf, c->written, c->cno);

    /** 如果发送到了长度，则删除事件，等读取完成再注册 **/
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
        islog_test("WHILE READ:[%s],cno[%ld]", c->buf, c->cno);

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

static struct option longopts[] = {
    { "threads",     required_argument, NULL, 't' },
    { "connections", required_argument, NULL, 'c' },
    { "duration",    required_argument, NULL, 'd' },
    { "latency",     no_argument,       NULL, 'L' },
    { "timeout",     required_argument, NULL, 'T' },
    { "test",       no_argument,       NULL, 'I' },
    { "trace",       no_argument,       NULL, 'S' },
    { "html",        optional_argument, NULL, 'H' },
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { NULL,          0,                 NULL,  0  }
};

static int parse_args(config * lcfg, int argc, char **argv) {
    int c;

    memset(lcfg, 0, sizeof(config));
    lcfg->threads     = 1;
    lcfg->connections = 1;
    lcfg->duration    = 10;
    lcfg->timeout     = SOCKET_TIMEOUT_MS;

    while ((c = getopt_long(argc, argv, "t:c:d:H:T:LIrv?", longopts, NULL)) != -1) {
        switch (c) {
            case 't':
                if (scan_metric(optarg, &lcfg->threads)) return -1;
                break;
            case 'c':
                if (scan_metric(optarg, &lcfg->connections)) return -1;
                break;
            case 'd':
                if (scan_time(optarg, &lcfg->duration)) return -1;
                break;
            case 'L':
                lcfg->islatency = 1;
                break;
            case 'T':
                if (scan_time(optarg, &lcfg->timeout)) return -1;
                lcfg->timeout *= 1000;
                break;
            case 'S':
                lcfg->istrace = 1;
                break;
            case 'H':
                lcfg->ishtml = 1;
                if( optarg != NULL){
                    strcpy( lcfg->htmlfile, optarg);
                }
                break;
            case 'I':
                lcfg->istest = 1;
                break;
            case 'v':
                printf("wrktcp version %s [%s] ", WRKVERSION, aeGetApiName());
                printf("Copyright (C) by [%s]\n", AUTHOR);
                break;
            case 'h':
            case '?':
            case ':':
            default:
                printf("command is error, input the illegal character:[%c]\n", c);
                return -1;
        }
    }

    if (optind == argc || !lcfg->threads || !lcfg->duration) return -1;

    strcpy( lcfg->tcpini.file, argv[optind]);
    /* 检查压测脚本是否符合要求 */
    if(strlen( lcfg->tcpini.file) >= TCPINIFILE_MAX_LENGTH || strlen(lcfg->tcpini.file) <= 0){
        fprintf(stderr, "filename's length is not allow [%ld]\n", strlen( lcfg->tcpini.file));
        return -1;
    }
    if( strstr( lcfg->tcpini.file, ".ini") == NULL){
        fprintf( stderr, "tcpini file must be ended with .ini,[%s]", lcfg->tcpini.file);
        return -1;
    }

    /* 检查连接和线程数的关系 */
    if (!lcfg->connections || lcfg->connections < lcfg->threads) {
        fprintf(stderr, "number of connections must be >= threads\n");
        return -1;
    }
    /* 检查htmlfile大小 */
    if( strlen( lcfg->htmlfile) > MAX_HTML_FILELEN){
        fprintf(stderr, "output html filename[%s] is too long. \n", lcfg->htmlfile);
        return -1;
    }

    return 0;
}

/* 执行和等待 */
static int running_sleep( struct config * lcfg ){
    uint64_t i ;
    uint64_t complete = 0;
    uint64_t failure = 0;
    uint64_t bytes = 0;
    uint64_t error = 0;

    lcfg->trace.step_time = lcfg->duration / TRACE_MAX_POINT;
    if( lcfg->duration % TRACE_MAX_POINT  > 0 ){
        lcfg->trace.step_time += 1;
    }
    /*** 开始等待lcfg->duration秒 ***/
    for( i =1; i <= lcfg->duration; i++){
        /** 收到打断信号, 则直接退出 **/
        if( stop == 1 ){
            lcfg->duration = (istime_us() - lcfg->result.tm_start) / 1000000 + 1;
            printf("\n");
            break;
        }
        /** 每秒刷新一次 **/
        sleep(1);

        /** 开始统计结果信息 **/
        complete = 0;
        failure = 0;
        bytes = 0;
        error = 0;
        for (int64_t j = 0; j < lcfg->threads; j++) {
            thread *t = &lcfg->p_threads[j];
            complete += t->complete;
            failure += t->errors.status;
            error += t->errors.connect + t->errors.read + t->errors.write + t->errors.timeout;
            bytes    += t->bytes;
        }

        uint64_t runtime_us = istime_us() - lcfg->result.tm_start;
        long double runtime_s   = runtime_us / 1000000.0;
        long double req_success_per_s = (complete - failure) / runtime_s;
        long double req_fail_per_s = failure / runtime_s;
        long double bytes_per_s = bytes      / runtime_s;

        printf("\r");

        printf("  Time:%"PRIu64"s", i);
        printf(" TPS:%.2Lf/%.2Lf Latency:", req_success_per_s, req_fail_per_s);
        printf("%s", format_time_us(lcfg->p_threads[0].latency));
        printf(" BPS:%sB", format_binary(bytes_per_s));
        printf(" Error:%"PRIu64"  ", error);

        /** 跟踪趋势点记录 **/
        int n = 0;
        if( lcfg->duration <= 100){
            lcfg->trace.step_time = 1;
            n = i-1;
        }else{
            /*, 如果>100个点，则需要隔点记录
            340秒 / 100个点 = 3;
            1 4 7 10 13 16 19 22 25 28 31 34 37 40
             */
            if( (i-1)%lcfg->trace.step_time == 0){
                n = (i-1)/lcfg->trace.step_time;
            }else{
                n = -1;
            }
        }
        if( n != -1){
            /* 抓取所有线程最大的响应时间 */
            int k;
            for( k = 0; k < lcfg->threads ; k++){
                if (lcfg->trace.latency[n] < lcfg->p_threads[k].latency){
                    lcfg->trace.latency[n] = lcfg->p_threads[k].latency;
                }
            }
            /* 记录tps并更新采集点tps最大值和最小值 */
            lcfg->trace.tps[n] = req_success_per_s;
            if( lcfg->result.tps_min > lcfg->trace.tps[n]  || lcfg->result.tps_min == 0 ){
                lcfg->result.tps_min  = lcfg->trace.tps[n];
            }
            if( lcfg->result.tps_max < lcfg->trace.tps[n] ){
                lcfg->result.tps_max  = lcfg->trace.tps[n];
            }
            cfg.trace.use_num++;
        }

        /*** 如果达到结尾，则换行 ***/
        if( i == lcfg->duration){
            printf("\n");
        }

        /*** 将标准输出打印出去 ***/
        fflush(stdout);
    }

    return 0;
}
static int response_complete(void * data, char * buf, size_t n) {
    connection *c = data;
    thread *thread = c->thread;

    if(c->rsp_state != COMPLETE){
        return 0;
    }

    uint64_t now = istime_us();

    thread->complete++;
    thread->requests++;

    /** 匹配结果，失败的要单独统计 **/
    islog_debug("rsp_head[%s], rsp_body[%s]", c->rsp_head, c->rsp_body);
    islog_test("RESPONSE_HEAD:[%s],RESPONSE_BODY[%s],cno[%ld]", c->rsp_head, c->rsp_body, c->cno);

    if(tcpini_response_issuccess(c->tcpini, c->rsp_head, c->rsp_body)){
        islog_debug(" response is success !!!!");
        /*EMPTY*/;
    }else{
        islog_debug(" response is failure !!!!");
        thread->errors.status++;
    }

    if (--c->pending == 0) {
        thread->latency = now - c->start;
        if (!stats_record(cfg.statistics.latency, now - c->start)) {
            thread->errors.timeout++;
        }
        c->delayed = cfg.isdelay;
        aeCreateFileEvent(thread->loop, c->fd, AE_WRITABLE, socket_writeable, c);
    }

    if( c->rsp_head != NULL){
        zfree(c->rsp_head);
        c->rsp_head = NULL;
    }
    if( c->rsp_body != NULL) {
        zfree(c->rsp_body);
        c->rsp_body = NULL;
    }

    c->rsp_state = HEAD;
    memset( c->buf, 0x00, sizeof(c->buf));

    /** tcp非长连接,需要重新连接 **/
    reconnect_socket(thread, c);

    return 0;
}

