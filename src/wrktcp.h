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
/** �������ʱ�� **/
#define RECORD_INTERVAL_MS  100
/** �����ټ�¼�� **/
#define TRACE_MAX_POINT 100
/** ����ļ���󳤶� **/
#define MAX_HTML_FILELEN 512


typedef struct {
    pthread_t thread;
    int tno;
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
    thread *thread;
    /** �������� **/
    long cno;
    int fd;
    bool delayed;
    uint64_t start; /*ͳ��ʱ�䣬��ʼ*/
    tcpini *tcpini;
    /** �������� **/
    char *request; /*��������*/
    long length; /*���󳤶�*/
    long written; /*�ѷ��ͳ���*/
    uint64_t pending; /*pipeline*/
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

/** wrk������������� **/
typedef struct config {
    uint64_t connections;   //������
    uint64_t duration;      //����ʱ��-��
    uint64_t threads;       //�߳���
    uint64_t timeout;       //��ʱʱ��
    uint64_t pipeline;      //��η��ͣ���ʱû����
    char isdelay;         //�Ƿ� �ӳٷ���
    char isdynamic;       //�Ƿ� �ж�̬ģ��
    char islatency;       //�Ƿ� ��ӡ��Ӧʱ��
    char istrace;         //�Ƿ� ��ӡtrace��ϸ
    char ishtml;          //�Ƿ� ����ļ�
    char htmlfile[MAX_HTML_FILELEN];     //���html�ļ���
    thread * p_threads;     //�̵߳�����
    /** tcp���ĵĽṹ������Ϣ **/
    tcpini  tcpini;
    /*** �����¼�����ܵĽ������ ***/
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
    }result;
    /** �ֲ�ͳ��,����ƽ��ֵ����� **/
    struct _statistics{
        stats *latency;
        stats *requests;
    } statistics;
    /** ׷��ͳ�ƣ���¼ʱ��仯���� **/
    struct _trace{
        int64_t step_time;
        double tps[TRACE_MAX_POINT];
        double latency[TRACE_MAX_POINT];
    }trace;
} config;

#endif /* WRKTCP_H */
