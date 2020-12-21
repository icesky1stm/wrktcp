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

/** ���֧�ֵ�TPS/10, Ŀǰ���� 100W **/
#define MAX_THREAD_RATE_S   10000000
/** ��ʱʱ��,���õ�Խ��ռ�õĿռ�Խ�� **/
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
    uint64_t start; /*ͳ��ʱ�䣬��ʼ*/
    char *request; /*��������*/
    long length; /*���󳤶�*/
    long written; /*�ѷ��ͳ���*/
    uint64_t pending; /*pipeline*/
    tcpini *tcpini;
    /** Ӧ������ **/
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
