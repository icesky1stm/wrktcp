//
// Created by icesky on 2020/12/11.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "tcpini.h"
#include "zmalloc.h"
#include "islog.h"
#include "wrktcp.h"

#define INI_LENGTH "$(length)"
static char *strtrim(char *str);

/***********************************
 * һ�����ü��ز���
 ***********************************/
static int tcpini_check( tcpini * tcpini);
static int tcpini_init( tcpini * tcpini);
static int line_parser(char *line, tcpini * tcpini);

/** ���������ļ��������� **/
int tcpini_file_load(char * filename, tcpini * tcpini ){
    FILE *fp = NULL;
    char *line = NULL;
    size_t line_len = 0;
    size_t line_size = 1024;

    if( filename == NULL  && strlen(filename) == 0){
        islog_error("tcpini file is error, please check");
        return -5;
    }
    fp = fopen( filename, "r");
    if( fp == NULL){
        islog_error("tcpini file[%s] open error,[%s]", filename, strerror(errno));
        return -5;
    }

    line = zcalloc( line_size * sizeof(char));
    if( line == NULL){
        islog_error("line malloc is error [%s]", strerror(errno));
        goto error;
    }

    /** ��ʼ���������ڴ� **/
    if( tcpini_init( tcpini) != 0){
        islog_error("tcpini_init is error ");
        goto error;
    }

    /** ѭ����ȡ�ļ�����,���д���ͽ��� **/
    memset( line, 0x00, sizeof(line_size));
    while( fgets( line+line_len , line_size - line_len, fp) != NULL){
        /*** �ȶ������ļ������� ***/
        if( line[0] == '#'){
            line_len = 0;
            continue;
        }
        /*** �滻����\nΪ�������������\n�Ļ� ***/
        line_len = strlen(line);
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }else{
            /*** �������\n����˵���ܳ���ָ�������ٴζ�ȡ ***/
            line_size = line_size * 2;
            line = zrealloc(line, line_size);
            if( line == NULL){
                islog_error("can't realloc line size, [%s]", strerror(errno));
                goto error;
            }
            /* ��չ���Ⱥ�, ������ȡ�������� */
            continue;
        }
        /*** ���ж���Ҫ���� ***/
        if( line[0] == '\0') {
            line_len = 0;
            continue;
        }
        strtrim(line);

        if(line_parser(line, tcpini) != 0){
            islog_error("parser line error!!!");
            goto error;
        }

        /*** �ָ�ѭ���ؼ��ֶ�,λ�� ***/
        line_len = 0;
   }

    /*** У�������Ƿ���ȷ ***/
    if( tcpini_check(tcpini) != 0){
        islog_error("tcpini config check error!!!");
        goto error;
    }

    fclose(fp);
    fp = NULL;
    return 0;

  error:
    fclose(fp);
    fp = NULL;
    return -5;
}

/* �������ݼ�� */
static int tcpini_check( tcpini * tcpini){
    /** У�������Ϣ�ĺϷ��� **/
    if(tcpini->host == NULL || strlen(tcpini->host) < 6){
        islog_error("item host mush be config!!!");
        return -30;
    }
    if(tcpini->port <= 0){
        islog_error("item host mush be config!!!");
        return -30;
    }

    if(tcpini->req_len_len < 1 ){
        islog_error("item req_len_len can't less than 1, current is %ld ", tcpini->req_len_len);
        return -30;
    }
    if(tcpini->req_body == NULL){
        islog_error("item req_body must be config!!!");
        return -30;
    }
    if( tcpini->rsp_headlen <= 1){
        islog_error("item rsp_headlen can't less than 1, current is %ld ", tcpini->rsp_headlen);
        return -30;
    }
    if( tcpini->rsp_len_beg < 1){
        islog_error("item rsp_len_beg can't less than 1, current is %ld ", tcpini->rsp_len_beg);
        return -30;
    }
    if( tcpini->rsp_len_len > 10) {
        islog_error("item rsp_len_len can't morel than 10, current is %ld ", tcpini->rsp_len_len);
        return -30;
    }
    if( strlen( tcpini->rsp_code_success) == 0){
        islog_error("item rsp_code_success can't be null, current is %s ", tcpini->rsp_code_success);
        return -30;
    }
    if( strlen( tcpini->rsp_code_localtion_tag) == 0){
        islog_error("item rsp_code_localtion_tag can't be null, current is %s ", tcpini->rsp_code_localtion_tag);
        return -30;
    }

    return 0;
}

/* ������Ϣ��ʼ�� */
static int tcpini_init( tcpini * tcpini){

    /** �����ڴ� **
    tcpini = zcalloc(1 * sizeof(tcpini));
    if(tcpini == NULL){
        islog_error("can't zcalloc memroy for load ini content, [%s]", strerror(errno));
        return -5;
    }
    */

    /** �趨��ʼֵ **/
    tcpini->section = S_COMMON;
    tcpini->req_len_len = 8;
    tcpini->req_len_type = TCPINI_REQ_LENTYPE_BODY;
    tcpini->req_head = zcalloc(strlen(INI_LENGTH) + 1);
    if(tcpini->req_head == NULL){
        islog_error("can't calloc memory,[%s]!!!", strerror(errno));
        return -5;
    }
    strcpy( tcpini->req_head, INI_LENGTH);

    tcpini->rsp_headlen = 8;
    tcpini->rsp_len_beg = 1;
    tcpini->rsp_len_len = 8;
    tcpini->rsp_len_type = TCPINI_RSP_LENTYPE_BODY;
    tcpini->rsp_code_type = TCPINI_PKG_FIXED;
    tcpini->rsp_code_location = TCPINI_RCL_HEAD;
    strcpy(tcpini->rsp_code_success, "000000");
    strcpy(tcpini->rsp_code_localtion_tag, "1 6");

    return 0;
}

/* ���������ļ��ľ����� */
static int line_parser(char *line, tcpini * tcpini){
    char name[32];
    char * pline = NULL;

    memset( name, 0x00, sizeof( name));
    /** �˲���д���������zlog��conf�Ĵ���ģʽ **/

    /*** ���ǲ��� section ***/
    if( line[0] == '[') {
        sscanf( line, "[ %[^] \t]", name);
        if ( strcmp( name, "common") == 0){
            tcpini->section = S_COMMON;
        } else if ( strcmp( name, "request") == 0){
            tcpini->section = S_REQUEST;
        } else if ( strcmp( name, "response") == 0){
            tcpini->section = S_RESPONSE;
        } else if ( strcmp( name, "parameters") == 0){
            tcpini->section = S_PARAMETERS;
        } else {
            islog_error("item is not support [%s]", name);
            return -5;
        }
        return 0;
    }

    /* ��ȡ������item������ */
    sscanf( line, " %[^=]", name);
    strtrim(name);
    /* ��ȡ�������ֵ����ȥǰ��ո� */
    pline = NULL;
    pline = strstr( line, "=");
    if( pline == NULL){
        islog_error("ini line must has '=', please check this line[%s]", line);
        return -5;
    }
    pline = pline + 1;
    strtrim( pline);

    switch(tcpini->section){
        case S_COMMON:
            if( strcmp( name, "host") == 0){
                /** ip **/
                tcpini->host = zcalloc(strlen(pline) + 1);
                if(tcpini->host == NULL){
                    islog_error("zcalloc memory error [%s]", strerror(errno));
                    return -5;
                }
                strcpy(tcpini->host, pline);
                if(strlen(tcpini->host) < 6){
                    islog_error("item[%s] is incorrect: [%s]",name, pline);
                    return -5;
                }
            } else if( strcmp( name, "port") == 0){
                /** port **/
                tcpini->port = atol(pline);
                if(tcpini->port <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else {
                islog_error("illegal item name [%s], please check", name);
                return -5;
            }
            break;
        case S_REQUEST:
            if( strcmp( name, "req_len_len") == 0){
                /** ����ͷ req_len_len **/
                tcpini->req_len_len = atol(pline);
                if(tcpini->req_len_len <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "req_len_type") == 0){
                /** ����ͷ�������� **/
                if( strcmp( pline, "total") == 0){
                    tcpini->req_len_type = TCPINI_REQ_LENTYPE_TOTAL;
                }else if( strcmp( pline, "body") == 0){
                    tcpini->req_len_type = TCPINI_REQ_LENTYPE_BODY;
                }else{
                    islog_error("item[%s] is incorrect: [%s],  legal value is total/body",name, line);
                    return -5;
                }
            } else if( strcmp( name, "req_head") == 0) {
               /* �����ٳ�ʼ����req_head */
               zfree(tcpini->req_head);
                tcpini->req_head = zcalloc(strlen(pline) + 1);
               if(tcpini->req_head == NULL){
                   islog_error("calloc req_head error [%s]", strerror(errno));
                   return -5;
               }
               strcpy(tcpini->req_head, pline);
            } else if( strcmp( name, "req_body") == 0){
                tcpini->req_body = zcalloc((strlen(pline) + 1));
               if(tcpini->req_body == NULL){
                   islog_error("calloc req_head error [%s]", strerror(errno));
                   return -5;
               }
               strcpy( tcpini->req_body, pline);
            } else {
                islog_error("request's item is error [%s]", name);
                return -5;
            }
            break;
        case S_RESPONSE:
            if( strcmp( name, "rsp_headlen") == 0){
                tcpini->rsp_headlen = atol(pline);
                if(tcpini->rsp_headlen <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_len_beg") == 0){
                tcpini->rsp_len_beg = atol(pline);
                if(tcpini->rsp_len_beg <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_len_len") == 0){
                tcpini->rsp_len_len = atol(pline);
                if(tcpini->rsp_len_len <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_len_type") == 0){
                if( strcmp( pline, "total") == 0){
                    tcpini->rsp_len_type = TCPINI_RSP_LENTYPE_TOTAL;
                }else if( strcmp( pline, "body") == 0){
                    tcpini->rsp_len_type = TCPINI_RSP_LENTYPE_BODY;
                }else{
                    islog_error("item[%s] is incorrect: [%s],  legal value is total/body",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_code_type") == 0){
                if( strcmp( pline, "fixed") == 0){
                    tcpini->rsp_code_type = TCPINI_PKG_FIXED;
                }else if( strcmp( pline, "xml") == 0){
                    tcpini->rsp_code_type = TCPINI_PKG_XML;
                }else if( strcmp( pline, "json") == 0){
                    tcpini->rsp_code_type = TCPINI_PKG_JSON;
                }else{
                    islog_error("item[%s] is incorrect: [%s],  legal value is fixed/xml.json",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_code_location") == 0){
                if( strcmp( pline, "head") == 0){
                    tcpini->rsp_code_location = TCPINI_RCL_BODY;
                }else if( strcmp( pline, "body") == 0){
                    tcpini->rsp_code_location = TCPINI_RCL_HEAD;
                }else{
                    islog_error("item[%s] is incorrect: [%s], legal value is head/body.",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_code_success") == 0){
                strcpy(tcpini->rsp_code_success, pline);
                if(strlen(tcpini->rsp_code_success) <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "rsp_code_localtion_tag") == 0){
                strcpy(tcpini->rsp_code_localtion_tag, pline);
                if(strlen(tcpini->rsp_code_localtion_tag) <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else {
                islog_error("response item is error [%s]", name);
                return -5;
            }
            break;
        case S_PARAMETERS:
            //TODO
            break;
        default:
            islog_error("section is not support");
            return -5;
    }
    return 0;
}

/***********************************
 * ����������ƴװ����
 ***********************************/
static int tcpini_para_eval(char * key, char * *value, tcpini * tcpini );
static int tcpini_buf_eval(char ** p_buf, int * p_len, char *tmpl_buf, tcpini *tcpini);

/** ����������Ϣ��Ϊ������Ϣ **/
int tcpini_request_parser(tcpini *tcpini, char ** request, long *length ){
    int true_headlen = 0;
    int true_bodylen = 0;
    char * true_head = NULL;
    char * true_body = NULL;
    char true_len[32];

    /** ��������ͷ���� **/
    if(tcpini_buf_eval( &true_head, &true_headlen, tcpini->req_head, tcpini) != 0){
        islog_error("tcpini buf eval req_head error!![%s]", tcpini->req_head);
        goto error;
    }

    /** ������������� **/
    if(tcpini_buf_eval( &true_body, &true_bodylen, tcpini->req_body, tcpini) != 0){
        islog_error("tcpini buf eval req_body error!![%s]", tcpini->req_body);
        goto error;
    }

    /** ��ʼƴװrequestҪ���͵��ܱ������� **/
    *request  = zcalloc( (true_headlen + true_bodylen) * sizeof(char));
    memcpy( *request, true_head, true_headlen);
    memcpy( *request+true_headlen, true_body, true_bodylen);

    /** request�ܳ��� **/
    *length =  true_headlen + true_bodylen;

    /** �滻�����е��ܳ���,����������Ϊ��֧��body�з�length����� **/
    char * p = NULL;
    p = strstr(*request , "$$");
    if( p == NULL){
        islog_error("request strstr $$ error!![%s]", *request);
        goto error;
    }
    memset( true_len, 0x00, sizeof( true_len));
    if(tcpini->req_len_type == TCPINI_REQ_LENTYPE_TOTAL){
        sprintf(true_len, "%0*d", tcpini->req_len_len, true_headlen + true_bodylen);
    }else{
        sprintf(true_len, "%0*d", tcpini->req_len_len, true_bodylen);
    }
    memcpy(p, true_len, tcpini->req_len_len);

    return 0;

    error:
    zfree(true_head);
    zfree(true_body);
    return -5;
}

/* ������������ֵ */
static int tcpini_para_eval(char * key, char * *value, tcpini * tcpini ){
    int i = 0;
    time_t rawtime = 0;
    struct tm * timeinfo;

    a_para * para = NULL;

    /** ���ĳ��������⴦�� **/
    if( strcmp( key, "length") == 0){
        *value = zcalloc(tcpini->req_len_len * sizeof(char));
        if( value == NULL){
            islog_error("calloc memroy error, [%s]", strerror(errno));
            return -5;
        }
        /*** ǰ��λ��$$������������Ҫ2λ�� ***/
        snprintf(*value, tcpini->req_len_len + 1, "$$%0*d", tcpini->req_len_len - 2, 0);
        return 0;
    }

    /** Ŀǰʹ�ñ����������������Ƕ�̬��չ��hashmap,ʹ�ÿ���Ѱַ��TODO **/
    for( i = 0; i < TCPINI_MAX_PARANUM ; i++){
        if(strcmp(tcpini->paras[i].key, key) == 0){
            para = &tcpini->paras[i];
            break;
        }
    }

    if( para == NULL){
        islog_error(" Can't find parameters $(%s), please check it has be config ", key);
        return -5;
    }

    switch(para->type){
        case TCPINI_P_COUNTER:
            //TODO
            //������Ҫȫ�־�̬��������������
            break;
        case TCPINI_P_FILE:
            //TODO
            break;
        case TCPINI_P_CONNECTID:
            *value =  zcalloc( 8 * sizeof(char));
            if( *value == NULL){
                islog_error("calloc memroy error, [%s]", strerror(errno));
                return -5;
            }
//            sprintf( *value, "%08ld", thread);
            break;
        case TCPINI_P_DATETIME:
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            *value = zcalloc( strlen(para->format) + 2);
            if( *value == NULL){
                islog_error("calloc memroy error, [%s]", strerror(errno));
                return -5;
            }
            strftime( *value, strlen(para->format+2), para->format, timeinfo);
            break;
    }

    return 0;
}

/* �����ַ����ı�����ֵ */
static int tcpini_buf_eval(char ** p_buf, int * p_len, char *tmpl_buf, tcpini *tcpini){
    char * p = NULL;
    char * q = NULL;
    char * value = NULL;
    char key[32];
    int buf_len ;
    char * buf;

    *p_buf = NULL;
    *p_len = 0;

    buf_len = strlen(tmpl_buf);
    /** �߽���������δ���ã���ֱ�ӷ���  **/
    if( buf_len == 0){
        buf = NULL;
        return 0;
    }

    /** Ϊbuf�����ʼ���ȣ�������Ҫrelloc **/
    buf = zcalloc(buf_len * sizeof(char));
    if( buf == NULL){
        islog_error(" calloc error [%s]", strerror(errno));
        return -5;
    }
    q = tmpl_buf;
    /** ѭ������$()���� **/
    while(1){
        /** ���ұ��� **/
        p = NULL;
        p = strstr(q, "$(");
        if( p == NULL){
            /** copy����tmplateβ�� **/
            memcpy( buf+strlen(buf), q, strlen(q));
            break;
        }
        /** copy ����ǰ׺ **/
        memcpy( buf+strlen(buf), q, p-q);

        /** Ѱ�Ҳ��������� **/
        q = strstr( p+2, ")");

        /** ȡ���������� **/
        memset( key, 0x00, sizeof( key));
        /* $(length) */
        memcpy( key, p+2, q - (p + 2) );

        /** ��ȡ����ʵ��ֵ **/
        if(tcpini_para_eval(key, &value, tcpini) != 0){
            return -5;
        }

        /** ����ʵ�ʳ���,���������Ҫ��̬�ĵ��� **/
        if( strlen(buf) + strlen(value) > buf_len){
            buf_len += strlen(value) - strlen(key) - 3;
            buf = zrealloc(buf, buf_len * sizeof(char));
            if( buf == NULL){
                return -5;
            }
        }
        /** copy����ʵ������ **/
        memcpy( buf+strlen(buf), value, strlen(value));

        q = q + 1;
    }

    *p_len = strlen(buf);
    *p_buf = buf;

    return 0;
}
/***********************************
 * ����Ӧ���Ľ�������
 ***********************************/
static int response_head(void * data, char * buf, size_t n);
static int response_body(void * data, char * buf, size_t n);

/** Ӧ�𲿷ִ���-��ʼ�� **/
int tcpini_response_init(void * data){
    connection *c = data;
    c->readlen = 0;
    c->rsp_head = zcalloc(c->tcpini->rsp_headlen * sizeof(char) + 1);
    if( c->rsp_head == NULL){
        return -5;
    }

    return 0;
}
/** Ӧ�𲿷ִ���-��� **/
int tcpini_response_parser(void * data, char * buf, size_t n){
    connection *c = data;

    c->readlen += n;

    if( response_head( c, buf, n) != 0){
        goto error;
    }

    if( response_body( c, buf, n) != 0){
        goto error;
    }

    return 0;
  error:
    return -5;

}
/** ������Ӧ���Ƿ�Ϊ�ɹ� **/
int tcpini_response_issuccess(tcpini *tcpini, char *head, char *body){
    int success = 0;
    char * rsp_code_pos = body;

    int code_beg = 0;
    int code_len = 0;
    char * p = NULL;
    char * q = NULL;

    char rsp_code[32];
    memset( rsp_code, 0x00, sizeof( rsp_code));

    /** ��Ӧ��Ĳ��� **/
    if(tcpini->rsp_code_location == TCPINI_RCL_HEAD){
        rsp_code_pos = head;
    }
    if(tcpini->rsp_code_location == TCPINI_RCL_BODY){
        rsp_code_pos = body;
    }

    switch(tcpini->rsp_code_type){
        /* �̶���ʽ */
        case TCPINI_PKG_FIXED:
            sscanf(tcpini->rsp_code_localtion_tag, "%d%d", &code_beg, &code_len);
            memcpy( rsp_code, rsp_code_pos+code_beg-1, code_len);

            break;
        case TCPINI_PKG_XML:
            /* eg:
             * <root><rspcode>000000</rspcode></root>
             *       0123456789
             * */
            p = strstr(rsp_code_pos, tcpini->rsp_code_localtion_tag);
            q = strstr( p + strlen(tcpini->rsp_code_localtion_tag), "</");
            memcpy(rsp_code, p + strlen(tcpini->rsp_code_localtion_tag), q - p - 1);

            break;
        case TCPINI_PKG_JSON:
            /* eg:
             * {��status��: ��0000��, ��message��: ��success��}
             * */
            p = strstr(rsp_code_pos, tcpini->rsp_code_localtion_tag);
            q = strstr( p + strlen(tcpini->rsp_code_localtion_tag), "\"");
            p = strstr( q+1, "\"");
            memcpy( rsp_code, q, p -q - 1);

            break;
        default:
            break;
    }

    if(strcmp(rsp_code, tcpini->rsp_code_success) == 0){
        success = 1;
    }

    return success;
}

static int response_head(void * data, char * buf, size_t n) {
    connection *c = data;

    char tmp_len[21];
    int head_left_len = 0;

    if( c->rsp_state != HEAD ){
        return 0;
    }

    if( c->readlen < c->tcpini->rsp_headlen){
        islog_debug(" readlen is less than rsp_headlen,  continue read");
        /** û���걨��ͷ����һֱ���� **/
        memcpy( c->rsp_head + strlen(c->rsp_head), buf, n);
    }
    else{
        /** ����ͷ�Ѿ��������ȣ���ʼ���� **/
        head_left_len = c->tcpini->rsp_headlen - strlen(c->rsp_head);

        memcpy( c->rsp_head + strlen(c->rsp_head), buf, head_left_len);

        memset( tmp_len, 0x00, sizeof( tmp_len));
        memcpy(tmp_len, c->rsp_head + c->tcpini->rsp_len_beg - 1, c->tcpini->rsp_len_len);
        c->rsp_len = atol( tmp_len);

        /** ���������뱨����ĳ��ȣ�����û�����жϣ������lentype��totoal����������˱���ͷ�ĳ��� **/
        c->rsp_body = zcalloc( c->rsp_len * sizeof(char) + 1 );
        if( c->rsp_body == NULL){
            fprintf(stderr, "rsp_body calloc error [%s]", strerror(errno));
            return -5;
        }
        memcpy( c->rsp_body, buf + head_left_len, c->readlen - c->tcpini->rsp_headlen);
        c->rsp_state = BODY;
    }

    return 0;
}

static int response_body(void * data, char * buf, size_t n) {
    connection *c = data;

    if( c->rsp_state != BODY){
        return 0;
    }

    if( strlen( c->rsp_body) < c->readlen - c->tcpini->rsp_headlen){
        memcpy( c->rsp_body + strlen(c->rsp_body), c->buf, n);
    }

    islog_debug("readlen[%d], rsplen[%d],rsp_headlen[%d]", c->readlen, c->rsp_len, c->tcpini->rsp_headlen);
    /** ���ĳ���Ϊtotalģʽ **/
    if(c->tcpini->rsp_len_type == TCPINI_RSP_LENTYPE_TOTAL)
    {
        /* ���ĳ��Ȳ�������ĳ��� */
        if( c->readlen < c->rsp_len) {
            ;
        }else if( c->readlen == c->rsp_len){
            c->rsp_state = COMPLETE;
        }else{
            islog_error("read body len:%ld is bigger than rsp_len:%ld, please check!!",
                        c->readlen, c->rsp_len);
            return -10;
        }
    }

    /** ����ͷ����Ϊbodyģʽ **/
    if(c->tcpini->rsp_len_type == TCPINI_RSP_LENTYPE_BODY) {
        /* ���ĳ��Ȳ�������ĳ��� */
        if (c->readlen < c->rsp_len + c->tcpini->rsp_headlen){
            ;
        }else if( c->readlen == c->rsp_len + c->tcpini->rsp_headlen){
            c->rsp_state = COMPLETE;
        }else{
            islog_error("read body len:%ld is bigger than rsp_len:%ld, please check!!",
                        c->readlen, c->rsp_len + c->tcpini->rsp_headlen);
            return -10;
        }
    }

    return 0;
}



/** ȥ���ַ�����ǰ��ո� **/
static char *strtrim(char *str)
{
    int i;
    int b1, e1;

    if (strlen(str) == 0) return str;

    if (str)
    {
        for(i = 0; str[i] == ' '; i++);

        b1 = i;

        for(i = strlen(str) - 1; i >= b1 && str[i] == ' '; i--);

        e1 = i;

        if (e1 >= b1)
            memmove(str, str+b1, e1-b1+1);

        str[e1-b1+1] = 0;
        return str;
    }
    else return str;
}


