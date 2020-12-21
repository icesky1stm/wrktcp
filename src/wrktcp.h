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

#define RECORD_INTERVAL_MS  100

typedef struct {
    pthread_t thread;
    int tno;
    aeEventLoop *loop;
    uint64_t connections;
    uint64_t complete;
    uint64_t requests;
    uint64_t bytes;
    uint64_t start;
    tcpini *tcpini;
    errors errors;
    struct connection *cs;
} thread;

typedef struct connection {
    thread *thread;
    long cno;
    int fd;
    bool delayed;
    uint64_t start; /*统计时间，开始*/
    char *request; /*请求整体*/
    long length; /*请求长度*/
    long written; /*已发送长度*/
    uint64_t pending; /*pipeline*/
    tcpini *tcpini;
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

#endif /* WRKTCP_H */
