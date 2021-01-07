//
// Created by icesky on 2020/12/11.
//

#ifndef TCPINI_H
#define TCPINI_H

#define TCPINI_MAX_PARANUM 20
#define TCPINIFILE_MAX_LENGTH 256

#include <pthread.h>

typedef struct _a_para{
    char key[32];
    void *offset;
    enum{
        TCPINI_P_COUNTER,
        TCPINI_P_FILE,
        TCPINI_P_CONNECTID,
        TCPINI_P_DATETIME
    }type;
    char *value;
    pthread_mutex_t mutex;
    char format[128];
} a_para;

typedef struct _tcpini_content {
    /** 基本信息 common **/
    char file[TCPINIFILE_MAX_LENGTH];
    char *host;
    int port;
    enum{
        S_COMMON,
        S_REQUEST,
        S_RESPONSE,
        S_PARAMETERS
    }section;

    /** 请求信息 request **/
    int req_len_len;
    char *req_head;
    char *req_body;
    enum {
        TCPINI_REQ_LENTYPE_BODY,
        TCPINI_REQ_LENTYPE_TOTAL
    }req_len_type;
    int req_headlen;
    int req_bodylen;

    /** 应答信息 response **/
    size_t rsp_headlen;
    size_t rsp_len_beg;
    size_t rsp_len_len;
    enum {
        TCPINI_RSP_LENTYPE_BODY,
        TCPINI_RSP_LENTYPE_TOTAL
    }rsp_len_type;
    enum {
        TCPINI_PKG_FIXED,
        TCPINI_PKG_XML,
        TCPINI_PKG_JSON
    }rsp_code_type;
    enum {
        TCPINI_RCL_BODY,
        TCPINI_RCL_HEAD
    }rsp_code_location;
    char rsp_code_success[32];
    char rsp_code_location_tag[32];

    /** 参数列表 parameters **/
    a_para paras[TCPINI_MAX_PARANUM];
    int paras_pos;

} tcpini;

int tcpini_file_load(char * filename, tcpini * tcpini );
int tcpini_request_parser(tcpini *tcpini, char **request, long *length, void * data);
int tcpini_response_init(void * data);
int tcpini_response_parser(void * data, char * buf, size_t n);
int tcpini_response_issuccess(tcpini *tcpini, char *head, char *body);


#endif //TCPINI_H
