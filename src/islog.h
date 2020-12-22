//
// Created by suitm on 2020/12/15.
//

#ifndef ISLOG_H
#define ISLOG_H

void islog_print(char *file, long line, char *level, char *fmtstr, ...);

/** ͨ����ķ�ʽ������־�Ƿ������
 * ��Ϊ��Ԥ����ʱִ��,��Ч����ȫû��Ӱ��
 * �����޸���Ҫ���±������õ�ͷ�ļ� **/
#define ISLOG_ERROR_ENABLE
#define ISLOG_WARN_ENABLE
#define ISLOG_INFO_ENABLE
/*
#define ISLOG_DEBUG_ENABLE
 */

/** ����������͵���־��� **/
#ifdef ISLOG_ERROR_ENABLE
#define islog_error(...)  \
do{                      \
    islog_print(__FILE__,__LINE__, "error", __VA_ARGS__);\
}while(0)
#else
#define islog_error(...)
#endif

#ifdef ISLOG_WARN_ENABLE
 #define islog_warn(...)    islog_print(__FILE__,__LINE__, "warn", __VA_ARGS__)
#else
 #define islog_warn(...)
#endif

#ifdef ISLOG_INFO_ENABLE
 #define islog_info(...)    islog_print(__FILE__,__LINE__, "info", __VA_ARGS__)
#else
 #define islog_info(...)
#endif

#ifdef ISLOG_DEBUG_ENABLE
 #define islog_debug(...)    islog_print(__FILE__,__LINE__, "debug", __VA_ARGS__)
#else
 #define islog_debug(...)
#endif


#endif //ISLOG_H
