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

#include "isstr.h"
#include "istime.h"

#include <pthread.h>

#define INI_LENGTH "$(length)"


/***********************************
 * 一、配置加载部分
 ***********************************/
static int tcpini_check( tcpini * tcpini);
static int tcpini_init( tcpini * tcpini);
static int line_parser(char *line, tcpini * tcpini);

/** 加载配置文件到配置中 **/
int tcpini_file_load(char * filename, tcpini * tcpini ){
    FILE *fp = NULL;
    char *line = NULL;
    int line_len = 0;
    int line_size = 1024;

    if( filename == NULL  && strlen(filename) == 0 && strstr(filename,".ini") != NULL){
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

    /** 初始化并申请内存 **/
    if( tcpini_init( tcpini) != 0){
        islog_error("tcpini_init is error ");
        goto error;
    }

    /** 循环读取文件内容,进行处理和解析 **/
    memset( line, 0x00, sizeof(line_size));
    while( fgets( line+line_len , line_size - line_len, fp) != NULL){
        /*** 先对配置文件做处理 ***/
        if( line[0] == '#'){
            line_len = 0;
            continue;
        }
        /*** 替换最后的\n为结束符，如果有\n并且不是\转义的话 ***/
        /*** TODO
         * 此处的写法有些复杂，应该追求简单，把每种情况列出来，不要在意代码冗余
         * 逻辑清晰的代码比看起来简洁的代码更重要
         * 一共分两类共4种场景：
         *  一、 读到了 \n
         *      1.1 是正常的结束， 加\0即可
         *      1.2 判断前面是否有\转义, 可能下一行还有,需要扩大内存重新读
         *  二、 未读到 \n
         *      2.1 单行太长，超过1024了，需要扩大内容重新读
         *      2.2 是文件最后一行，且没有换行, 需要直接就结束加\0
         */
        line_len = strlen(line);
        if (line[line_len - 1] == '\n' && line[line_len - 2] != '\\') {
            line[line_len - 1] = '\0';
        }else{
            /*** 如果不是\n或者有\\，则说明很长，指数扩容再次读取 ***/
            line_size = line_size * 2;
            line = zrealloc(line, line_size);
            if( line == NULL){
                islog_error("can't realloc line size, [%s]", strerror(errno));
                goto error;
            }
            /** 把 \\替换掉,支持\\转义换行 **/
            if( line[line_len - 2] == '\\' ){
                line[line_len - 2] = '\n';
                line[line_len - 1] = '\0';
                line_len = strlen(line);
            }
            /* 扩展长度后, 继续读取后续内容 */
            continue;
        }
        /*** 空行都需要跳过 ***/
        if( line[0] == '\0') {
            line_len = 0;
            continue;
        }
        isstr_trim(line);

        islog_debug("have read line [%s]", line);
        if(line_parser(line, tcpini) != 0){
            islog_error("parser line error!!!");
            goto error;
        }

        /*** 恢复循环关键字段,位移 ***/
        line_len = 0;
   }

    /*** 校验配置是否正确 ***/
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

/* 配置内容检查 */
static int tcpini_check( tcpini * tcpini){
    /** 校验各种信息的合法性 **/
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

/* 配置信息初始化 */
static int tcpini_init( tcpini * tcpini){
    /** 设定初始值 **/
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
    tcpini->rsp_code_location = TCPINI_RCL_BODY;
    strcpy(tcpini->rsp_code_success, "000000");
    strcpy(tcpini->rsp_code_localtion_tag, "1 6");

    return 0;
}

/* 解析配置文件的具体行 */
static int line_parser(char *line, tcpini * tcpini){
    char name[32];
    char * pline = NULL;
    char * q = NULL;
    memset( name, 0x00, sizeof( name));
    /** 此部分写法，借鉴了zlog的conf的处理模式 **/

    /*** 看是不是 section ***/
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

    /* 获取配置项item的名称 */
    sscanf( line, " %[^=]", name);
    isstr_trim(name);
    if( strlen(name) > 32){
        islog_error("ini item name length must less then 32");
        return -5;
    }

    /* 获取配置项的值并且去前后空格 */
    pline = NULL;
    pline = strstr( line, "=");
    if( pline == NULL){
        islog_error("ini line must has '=', please check this line[%s]", line);
        return -5;
    }
    pline = pline + 1;
    isstr_trim( pline);

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
                /** 报文头 req_len_len **/
                tcpini->req_len_len = atol(pline);
                if(tcpini->req_len_len <= 0){
                    islog_error("item[%s] is incorrect: [%s]",name, line);
                    return -5;
                }
            } else if( strcmp( name, "req_len_type") == 0){
                /** 报文头长度类型 **/
                if( strcmp( pline, "total") == 0){
                    tcpini->req_len_type = TCPINI_REQ_LENTYPE_TOTAL;
                }else if( strcmp( pline, "body") == 0){
                    tcpini->req_len_type = TCPINI_REQ_LENTYPE_BODY;
                }else{
                    islog_error("item[%s] is incorrect: [%s],  legal value is total/body",name, line);
                    return -5;
                }
            } else if( strcmp( name, "req_head") == 0) {
               /* 先销毁初始化的req_head */
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
                    tcpini->rsp_code_location = TCPINI_RCL_HEAD;
                }else if( strcmp( pline, "body") == 0){
                    tcpini->rsp_code_location = TCPINI_RCL_BODY;
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
            strcpy( tcpini->paras[tcpini->paras_pos].key, name);
            q = NULL;
            q = strstr( pline, ",");
            char para_type[20];
            memset( para_type, 0x00, sizeof( para_type));
            memcpy( para_type, pline, q - pline);
            if( strcmp( para_type, "COUNTER") == 0){
                tcpini->paras[tcpini->paras_pos].type = TCPINI_P_COUNTER;
                tcpini->paras[tcpini->paras_pos].offset = zcalloc(1*sizeof(long));
                pthread_mutex_init(&tcpini->paras[tcpini->paras_pos].mutex, NULL);
            }else if( strcmp( para_type, "FILE") == 0){
                tcpini->paras[tcpini->paras_pos].type = TCPINI_P_FILE;
                pthread_mutex_init(&tcpini->paras[tcpini->paras_pos].mutex, NULL);
            }else if( strcmp( para_type, "DATETIME") == 0){
                tcpini->paras[tcpini->paras_pos].type = TCPINI_P_DATETIME;
            }else if( strcmp( para_type, "CONNECTID") == 0){
                tcpini->paras[tcpini->paras_pos].type = TCPINI_P_CONNECTID;
            }else{
                islog_error("parameter type is error [%s], not in COUNTER/FILE/DATATIME/CONNECTID ",para_type);
            }
            strcpy( tcpini->paras[tcpini->paras_pos].format, q+1);
            isstr_trim(tcpini->paras[tcpini->paras_pos].format);
            tcpini->paras_pos++;
            break;
        default:
            islog_error("section is not support");
            return -5;
    }
    return 0;
}

/***********************************
 * 二、请求报文拼装部分
 ***********************************/
static int tcpini_para_eval(char * key, char * *value, tcpini * tcpini, void * data );
static int tcpini_buf_eval(char ** p_buf, int * p_len, char *tmpl_buf, tcpini *tcpini, void * data);

/** 解析配置信息成为发送信息 **/
int tcpini_request_parser(tcpini *tcpini, char ** request, long *length, void * data ){
    int true_headlen = 0;
    int true_bodylen = 0;
    char * true_head = NULL;
    char * true_body = NULL;
    char true_len[32];

    /** 解析报文头参数 **/
    if(tcpini_buf_eval( &true_head, &true_headlen, tcpini->req_head, tcpini, data) != 0){
        islog_error("tcpini buf eval req_head error!![%s]", tcpini->req_head);
        goto error;
    }

    /** 解析报文体参数 **/
    if(tcpini_buf_eval( &true_body, &true_bodylen, tcpini->req_body, tcpini, data) != 0){
        islog_error("tcpini buf eval req_body error!![%s]", tcpini->req_body);
        goto error;
    }

    /** 开始拼装request要发送的总报文内容 **/
    *request  = zcalloc( (true_headlen + true_bodylen) * sizeof(char));
    memcpy( *request, true_head, true_headlen);
    memcpy( *request+true_headlen, true_body, true_bodylen);

    /** request总长度 **/
    *length =  true_headlen + true_bodylen;

    /** 替换报文中的总长度,放在下面是为了支持body中放length的情况 **/
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

/* 整个字符串的变量求值 */
static int tcpini_buf_eval(char ** p_buf, int * p_len, char *tmpl_buf, tcpini *tcpini, void * data){
    char * p = NULL;
    char * q = NULL;
    char * value = NULL;
    char key[32];
    int buf_len ;
    char * buf;

    *p_buf = NULL;
    *p_len = 0;

    buf_len = strlen(tmpl_buf);
    /** 边界情况，如果未配置，则直接返回  **/
    if( buf_len == 0){
        buf = NULL;
        return 0;
    }

    /** 为buf申请初始长度，后续还要relloc **/
    buf = zcalloc(buf_len * sizeof(char));
    if( buf == NULL){
        islog_error(" calloc error [%s]", strerror(errno));
        return -5;
    }
    q = tmpl_buf;
    /** 循环查找$()参数 **/
    while(1){
        /** 查找变量 **/
        p = NULL;
        p = strstr(q, "$(");
        if( p == NULL){
            /** copy最后的tmplate尾部 **/
            memcpy( buf+strlen(buf), q, strlen(q));
            break;
        }
        /** copy 参数前缀 **/
        memcpy( buf+strlen(buf), q, p-q);

        /** 寻找参数结束符 **/
        q = strstr( p+2, ")");

        /** 取到变量名称 **/
        memset( key, 0x00, sizeof( key));
        /* $(length) */
        memcpy( key, p+2, q - (p + 2) );

        /** 获取变量实际值 **/
        if(tcpini_para_eval(key, &value, tcpini, data) != 0){
            return -5;
        }
        islog_debug("para eval [%s]=[%s]",key, value);

        /** 计算实际长度,根据情况都要动态的调整 **/
        if( strlen(buf) + strlen(value) > buf_len){
            buf_len += strlen(value) - strlen(key) - 3;
            buf = zrealloc(buf, buf_len * sizeof(char));
            if( buf == NULL){
                return -5;
            }
        }
        /** copy参数实际内容 **/
        memcpy( buf+strlen(buf), value, strlen(value));

        q = q + 1;
    }

    *p_len = strlen(buf);
    *p_buf = buf;

    return 0;
}

static int tcpini_para_type_datetime( char * key, char **value, a_para * para, tcpini * tcpini, void * data);
static int tcpini_para_type_connectid( char * key, char **value, a_para * para, tcpini * tcpini, void * data);
static int tcpini_para_type_counter( char * key, char **value, a_para * para, tcpini * tcpini, void * data);
static int tcpini_para_type_file( char * key, char **value, a_para * para, tcpini * tcpini, void * data);


/* 单个变量的求值 */
static int tcpini_para_eval(char * key, char * *value, tcpini * tcpini, void * data){
    int i = 0;

    a_para * para = NULL;

    /** 报文长度域特殊处理 **/
    if( strcmp( key, "length") == 0){
        *value = zcalloc(tcpini->req_len_len * sizeof(char));
        if( value == NULL){
            islog_error("calloc memroy error, [%s]", strerror(errno));
            return -5;
        }
        /*** 前两位是$$，长度至少需要2位长 ***/
        snprintf(*value, tcpini->req_len_len + 1, "$$%0*d", tcpini->req_len_len - 2, 0);
        return 0;
    }

    /** 目前使用遍历，后续可以做非动态扩展的hashmap,使用开放寻址法TODO **/
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

    int ret = 0;
    switch(para->type){
        case TCPINI_P_COUNTER:
            ret = tcpini_para_type_counter( key, value, para, tcpini, data);
            break;
        case TCPINI_P_FILE:
            ret = tcpini_para_type_file( key, value, para, tcpini, data);
            break;
        case TCPINI_P_CONNECTID:
            ret = tcpini_para_type_connectid( key, value, para, tcpini, data);
            break;
        case TCPINI_P_DATETIME:
            ret = tcpini_para_type_datetime( key, value, para, tcpini, data);
            break;
    }
    if( ret != 0){
        islog_error("para_type_deal error");
        return -5;
    }

    return 0;
}

/*  datetime类型的值获取 */
static int tcpini_para_type_datetime( char * key, char **value, a_para * para, tcpini * tcpini, void * data){
    islog_debug("para type is datetime --[%s]", key);
    *value = zcalloc( strlen(para->format) * 2 + 100);
    if( *value == NULL){
        islog_error("calloc memroy error, [%s]", strerror(errno));
        return -5;
    }
    istime_strftime( *value, strlen(para->format) * 2 + 100, para->format, istime_us());
    islog_debug("para type is datetime --[%s]", *value);
    return 0;
}
/*  connectid类型的值获取 */
static int tcpini_para_type_connectid( char * key, char **value, a_para * para, tcpini * tcpini, void * data){
    islog_debug("para type is connectid --[%s],[%x]", key, data);
    connection *c= data;
    char * format = para->format;

    /* 先算长度, snprintf的特殊用法 */
    int n = 0;
    n = snprintf(NULL, 0, format, c->cno) + 1;

    *value =  zcalloc( n * sizeof(char));
    if( *value == NULL){
        islog_error("calloc memroy error, [%s]", strerror(errno));
        return -5;
    }
    snprintf( *value, n, format, c->cno);
    islog_debug("para type is connectid --[%s]", *value);
    return 0;
}
/* counter类型的获取 */
static int tcpini_para_type_counter( char * key, char **value, a_para * para, tcpini * tcpini, void * data){
    islog_debug("para type is counter --[%s]", key);
    //connection *c= data;
    long l_start = 0;
    long l_end = 0;
    char element[20];
    char fmt[20];
    long l_step = 0;
    long *cur = NULL;
    long cur_value = 0;

    isstr_split( para->format, ",", 1, element);
    isstr_trim(element);
    l_start = atol( element);
    isstr_split( para->format, ",", 2, element);
    isstr_trim(element);
    l_end = atol( element);
    isstr_split( para->format, ",", 3, element);
    isstr_trim(element);
    l_step = atol( element);
    isstr_split( para->format, ",", 4, element);
    isstr_trim(element);
    strcpy( fmt, element);

    /** 此处多线程场景，需要进行原子操作 **/
    cur = para->offset;
    /* 加锁 */
    pthread_mutex_lock(&para->mutex);
    *cur = *cur + l_step;

    if( *cur > l_end ){
        *cur = *cur - l_end + l_start;
    }
    if( *cur < l_start){
        *cur = l_start;
    }
    cur_value = *cur;
    /* 去锁 */
    pthread_mutex_unlock(&para->mutex);

    /* 先算长度, snprintf的特殊用法 */
    int n = 0;
    n = snprintf(NULL, 0, fmt, cur_value) + 1;
    *value =  zcalloc( n * sizeof(char));
    if( *value == NULL){
        islog_error("calloc memroy error, [%s]", strerror(errno));
        return -5;
    }
    snprintf( *value, n, fmt, cur_value);

    islog_debug("para type is counter --[%s]", *value);
    return 0;
}
static int tcpini_para_type_file( char * key, char **value, a_para * para, tcpini * tcpini, void * data){
    islog_debug("para type is file --[%s]", key);
//    connection *c= data;
    FILE * fp = NULL;
    char * filename = para->format;

    if( para->offset == NULL){
        /** 第一次打开文件 **/
        pthread_mutex_lock(&para->mutex);
        if( para->offset == NULL){
            fp = fopen( filename, "r");
            if( fp == NULL){
                islog_error(" open para file[%s] error, [%s]", filename, strerror(errno));
                return -5;
            }
            para->offset = fp;
        }
        pthread_mutex_unlock(&para->mutex);
    }
    fp =  para->offset;

    if( *value == NULL){
        *value =  zcalloc( 1024 * sizeof(char));
        if( *value == NULL){
            islog_error("calloc memroy error, [%s]", strerror(errno));
            return -5;
        }
    }

    if( fgets( *value, 1024, fp) == NULL){
        islog_debug("fgets error, [%s]", strerror(errno));
        fseek( fp, 0, SEEK_SET);
        /** 可能是读到行尾了，重新打开继续读 **
        fclose(fp);
        fp = NULL;
        para->offset = NULL;
        */
    }
    islog_debug("branch.txt--[%s]", *value);
    if( (*value)[strlen(*value) - 1] == '\n'){
        (*value)[strlen(*value) - 1] = '\0';
    }

    islog_debug("para type is file --[%s]", *value);
    return 0;
}


/***********************************
 * 三、应答报文解析部分
 ***********************************/
static int response_head(void * data, char * buf, size_t n);
static int response_body(void * data, char * buf, size_t n);

/** 应答部分处理-初始化 **/
int tcpini_response_init(void * data){
    connection *c = data;
    c->readlen = 0;
    c->rsp_head = zcalloc(c->tcpini->rsp_headlen * sizeof(char) + 1);
    if( c->rsp_head == NULL){
        return -5;
    }

    return 0;
}
/** 应答部分处理-拆包 **/
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
/** 解析响应码是否为成功 **/
int tcpini_response_issuccess(tcpini *tcpini, char *head, char *body){
    int success = 0;
    char * rsp_code_pos = body;

    int code_beg = 0;
    int code_len = 0;
    char * p = NULL;
    char * q = NULL;

    char rsp_code[32];
    memset( rsp_code, 0x00, sizeof( rsp_code));

    /** 响应码的部分 **/
    if(tcpini->rsp_code_location == TCPINI_RCL_HEAD){
        rsp_code_pos = head;
    }
    if(tcpini->rsp_code_location == TCPINI_RCL_BODY){
        rsp_code_pos = body;
    }

    switch(tcpini->rsp_code_type){
        /* 固定格式 */
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
            if( p == NULL){
                islog_debug("rsp msg [%s] can't find [%s]", rsp_code_pos, tcpini->rsp_code_localtion_tag);
                strcpy( rsp_code, "");
                break;
            }
            q = strstr( p + strlen(tcpini->rsp_code_localtion_tag), "</");
            if( q == NULL){
                islog_debug("rsp msg [%s] can't find [%s]", rsp_code_pos, tcpini->rsp_code_localtion_tag);
                strcpy( rsp_code, "");
                break;
            }
            memcpy(rsp_code, p + strlen(tcpini->rsp_code_localtion_tag), q - (p+strlen(tcpini->rsp_code_localtion_tag)));

            break;
        case TCPINI_PKG_JSON:
            /* eg:
             * {“status”: “0000”, “message”: “success”}
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
    }else{
        islog_debug("rsp_code[%s] != [%s] ", rsp_code, tcpini->rsp_code_success);
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
        /** 没读完报文头，则一直处理 **/
        memcpy( c->rsp_head + strlen(c->rsp_head), buf, n);
    }
    else{
        /** 报文头已经读够长度，开始解析 **/
        head_left_len = c->tcpini->rsp_headlen - strlen(c->rsp_head);

        memcpy( c->rsp_head + strlen(c->rsp_head), buf, head_left_len);

        memset( tmp_len, 0x00, sizeof( tmp_len));
        memcpy(tmp_len, c->rsp_head + c->tcpini->rsp_len_beg - 1, c->tcpini->rsp_len_len);
        c->rsp_len = atol( tmp_len);

        /** 按长度申请报文体的长度，这里没有做判断，如果是lentype是totoal，则多申请了报文头的长度 **/
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
    /** 报文长度为total模式 **/
    if(c->tcpini->rsp_len_type == TCPINI_RSP_LENTYPE_TOTAL)
    {
        /* 读的长度不够定义的长度 */
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

    /** 报文头长度为body模式 **/
    if(c->tcpini->rsp_len_type == TCPINI_RSP_LENTYPE_BODY) {
        /* 读的长度不够定义的长度 */
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


