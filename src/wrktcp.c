// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include "main.h"
#include "wrktcp.h"

/** wrk������������� **/
config cfg;

/** wrk��ôд���������������ԣ��������� **/
static struct sock sock = {
        .connect  = sock_connect,
        .close    = sock_close,
        .read     = sock_read,
        .write    = sock_write,
        .readable = sock_readable
};

/**  ctrl +c ����ź� **/
static volatile sig_atomic_t stop = 0;
static void handler(int sig) {
    stop = 1;
}

/** ʹ��˵�� **/
static void usage() {
    printf("Usage: wrktcp <options> filename                      \n"
           "  Necessary Options:                                  \n"
           "    -c, --connections <N>  Connections to keep open   \n"
           "    -d, --duration    <T>  Duration of test           \n"
           "    -t, --threads     <N>  Number of threads to use   \n"

           "  Condition Options:                                  \n"
           "        --latency          Print latency statistics   \n"
           "        --timeout     <T>  Socket/request timeout     \n"
           "        --trace            Print tps/latency trace    \n"
           "        --html             output a html chart        \n"

           "    -v, --version          Print version details      \n"
           "                                                      \n"
           "  Numeric arguments may include a SI unit (1k, 1M, 1G)\n"
           "  Time arguments may include a time unit (2s, 2m, 2h) \n"
           "  for example:                                        \n"
           "    wrktcp -t4 -c200 -d30s sample_tiny.ini            \n"
           );
}

/*********************************************************
 * wrktcp ������
 * @param argc
 * @param argv
 * @return
 *********************************************************/
int main(int argc, char **argv) {
    /** �����źź��� **/
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  SIG_IGN);

    /** ������ڲ��� **/
    if (parse_args(&cfg, argc, argv)) {
        islog_error("argv is errror!!!");
        usage();
        exit(1);
    }

    /** ����ͳ����Ϣ��ʼֵ **/
    cfg.statistics.latency  = stats_alloc(cfg.timeout * 1000);
    cfg.statistics.requests = stats_alloc(MAX_THREAD_RATE_S);
    cfg.p_threads = zcalloc(cfg.threads * sizeof(thread));

    /** ����tcpinifile�ļ� **/
    if(tcpini_file_load(cfg.tcpini.file, &cfg.tcpini) != 0){
        fprintf(stderr, "unable to load tcpinifile %s\n",cfg.tcpini.file);
        exit(1);
    }

    /** ��ʼ�����߳� **/
    for (uint64_t i = 0; i < cfg.threads; i++) {
        thread *t      = &cfg.p_threads[i];
        t->tno = i + 1;
        t->loop        = aeCreateEventLoop(10 + cfg.connections * 3);
        t->connections = cfg.connections / cfg.threads;

        t->tcpini = &cfg.tcpini;
        if (i == 0) {
            /* �ܵ�����,Ŀǰһ������һ��ͨѶ */
            cfg.pipeline = 1;
            /* ��̬ģ�� */
            if(cfg.tcpini.paras->value != NULL){
                cfg.isdynamic = 1;
            }
            /* �ӳٷ��ͣ��ݲ�֧�� */
            cfg.isdelay = 0;
        }

        if (!t->loop || pthread_create(&t->thread, NULL, &thread_main, t)) {
            char *msg = strerror(errno);
            fprintf(stderr, "unable to create thread %"PRIu64": %s\n", i, msg);
            exit(2);
        }
    }

    /** ����ctrl+c����ź� **/
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    /** ��ӡ���ͷ **/
    printf("Running %s loadtest ", format_time_s(cfg.duration));
    printf("@ %s:%d ", cfg.tcpini.host, cfg.tcpini.port);
    printf("using %s", cfg.tcpini.file);
    printf("\n");
    printf("  %"PRIu64" threads and %"PRIu64" connections\n", cfg.threads, cfg.connections);

    /** ��ʼ��������Ϣ **/
    cfg.result.tm_start   = istime_us();
    cfg.result.complete = 0;
    cfg.result.bytes = 0;
    memset( &cfg.result.errors, 0x00, sizeof(errors));

    /** �����̵ȴ�������ֹͣ��־ **/
    running_sleep( &cfg);
    stop = 1;

    /** ��ʼ��¼ͳ�ƽ����Ϣ **/
    for (uint64_t j = 0; j < cfg.threads; j++) {
        thread *t = &cfg.p_threads[j];
        /* �����̵߳ȴ� */
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

    /** Coordinated Omission ״̬����, ����Ϊ��Ϲ����Ӧ��ʹ��wrk2�ķ���������Ϊ�˱��ֺ�wrk�Ľ��һ����ʱ���� **/
    if (cfg.result.complete / cfg.connections > 0) {
        /* ����һ������ƽ����ʱ���� */
        int64_t interval = cfg.result.runtime_us / (cfg.result.complete / cfg.connections);
        /* ����������Ӧ��ѧϰwrk2������-R���� */
        stats_correct(cfg.statistics.latency, interval);
    }

    /** �������Ļ�� **/
    output_console(&cfg);

    /** �����html�ļ��� **/
    if( cfg.ishtml ){
        output_html(&cfg);
    }

    return 0;
}

/** ���̵߳�������ʹ��epoll�����͹��������� **/
void *thread_main(void *arg) {
    thread *thread = arg;

    /** ������Ƕ�̬ģ�壬��ֻ��ʼ��һ�η�����Ϣ,���Ч�� **/
    char *request = NULL;
    long length = 0;
    if ( !cfg.isdynamic ) {
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
        c->delayed = cfg.isdelay;
        c->tcpini = thread->tcpini;
        islog_debug("CREATE c->no:ld", c->cno);
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
        /** �˴��޸���ͳ�Ʒ�ʽ��wrk�ķ�ʽ�ǰ�TPSΪ����ͳ�ƣ������1000Wtps��
         * ����������������⣺
         * 1.10000000 * uint64 = 80M���ڴ�ռ��
         * 2.���TPS�ϵͣ�������Ӧʱ����100ms���ϵ�ϵͳ��������TPSͳ�Ʒǳ��Ĳ�׼ȷ,��Ϊˢ�¼����100MS�����´���TPS�ֲ�����0
         * ����TPS���ȴ�1->0.1,Ϊ�˲������ڴ����ģ�����ʹ��1000W���ֲ��������൱�����������TPS��֧�ֵ�100W TPS
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

    switch (sock.connect(c, cfg.tcpini.host)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }

    /** ��ʼ�����յ���Ϣ **/
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

    /** ����0��ʱ�򣬾���Ҫ���͵�ʱ�� **/
    if (!c->written) {
        /*** ����Ƕ�̬ģ��,��ÿ��д��ʱ�򶼳�ʼ��,Ч�ʲ��� ***/
        if (cfg.isdynamic) {
            if(tcpini_request_parser( &cfg.tcpini, &c->request, &c->length) != 0){
                fprintf(stderr, "writer parser ini content error !!!");
                goto error;
            }
        }
        c->start   = istime_us();
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
    /** ������͵��˳��ȣ���ɾ��ʱ�䣬�ȶ�ȡ�����ע�� **/
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

        /** ������Ĵ�������ˣ������ͳ�Ƶȴ��� **/
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
    { "trace",       no_argument,       NULL, 'S' },
    { "html",        optional_argument, NULL, 'H' },
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { NULL,          0,                 NULL,  0  }
};

static int parse_args(config * lcfg, int argc, char **argv) {
    int c;

    memset(lcfg, 0, sizeof(config));
    lcfg->threads     = 2;
    lcfg->connections = 10;
    lcfg->duration    = 10;
    lcfg->timeout     = SOCKET_TIMEOUT_MS;

    while ((c = getopt_long(argc, argv, "t:c:d:H:T:Lrv?", longopts, NULL)) != -1) {
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
            case 'v':
                printf("wrktcp version %s [%s] ", WRKVERSION, aeGetApiName());
                printf("Copyright (C) 2020 icesky\n");
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
    /* ���ѹ��ű��Ƿ����Ҫ�� */
    if(strlen( lcfg->tcpini.file) >= TCPINIFILE_MAX_LENGTH || strlen(lcfg->tcpini.file) <= 0){
        fprintf(stderr, "filename's length is not allow [%ld]\n", strlen( lcfg->tcpini.file));
        return -1;
    }
    if( strstr( lcfg->tcpini.file, ".ini") == NULL){
        fprintf( stderr, "tcpini file must be ended with .ini,[%s]", lcfg->tcpini.file);
        return -1;
    }

    /* ������Ӻ��߳����Ĺ�ϵ */
    if (!lcfg->connections || lcfg->connections < lcfg->threads) {
        fprintf(stderr, "number of connections must be >= threads\n");
        return -1;
    }
    /* ���htmlfile��С */
    if( strlen( lcfg->htmlfile) > MAX_HTML_FILELEN){
        fprintf(stderr, "output html filename[%s] is too long. \n", lcfg->htmlfile);
        return -1;
    }

    return 0;
}

/* ִ�к͵ȴ� */
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
    /*** ��ʼ�ȴ�lcfg->duration�� ***/
    for( i =1; i <= lcfg->duration; i++){
        /** �յ�����ź�, ��ֱ���˳� **/
        if( stop == 1 ){
            lcfg->duration = (istime_us() - lcfg->result.tm_start) / 1000000 + 1;
            printf("\n");
            break;
        }
        /** ÿ��ˢ��һ�� **/
        sleep(1);

        /** ��ʼͳ�ƽ����Ϣ **/
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

        /** �������Ƶ��¼ **/
        int n = 0;
        if( lcfg->duration <= 100){
            lcfg->trace.step_time = 1;
            n = i-1;
        }else{
            /*, ���>100���㣬����Ҫ�����¼
            340�� / 100���� = 3;
            1 4 7 10 13 16 19 22 25 28 31 34 37 40
             */
            if( (i-1)%lcfg->trace.step_time == 0){
                n = (i-1)/lcfg->trace.step_time;
            }else{
                n = -1;
            }
        }
        if( n != -1){
            /* ץȡ�����߳�������Ӧʱ�� */
            int k;
            for( k = 0; k < lcfg->threads ; k++){
                if (lcfg->trace.latency[n] < lcfg->p_threads[k].latency){
                    lcfg->trace.latency[n] = lcfg->p_threads[k].latency;
                }
            }
            /* ��¼tps�����²ɼ���tps���ֵ����Сֵ */
            lcfg->trace.tps[n] = req_success_per_s;
            if( lcfg->result.tps_min > lcfg->trace.tps[n]  || lcfg->result.tps_min == 0 ){
                lcfg->result.tps_min  = lcfg->trace.tps[n];
            }
            if( lcfg->result.tps_max < lcfg->trace.tps[n] ){
                lcfg->result.tps_max  = lcfg->trace.tps[n];
            }
            cfg.trace.use_num++;
        }

        /*** ����ﵽ��β������ ***/
        if( i == lcfg->duration){
            printf("\n");
        }

        /*** ����׼�����ӡ��ȥ ***/
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

    /** ƥ������ʧ�ܵ�Ҫ����ͳ�� **/
    islog_debug("rsp_head[%s], rsp_body[%s]", c->rsp_head, c->rsp_body);
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

    zfree(c->rsp_head);
    zfree(c->rsp_body);
    c->rsp_state = HEAD;
    memset( c->buf, 0x00, sizeof(c->buf));

    /** tcp�ǳ�����,��Ҫ�������� **/
    reconnect_socket(thread, c);

    return 0;
}