//
// Created by suitm on 2020/12/15. 本来想引用islog的，由于太复杂，还是算了
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "islog.h"

#include <stdarg.h>

/* 最大输出行数 */
#define ISLOG_LINE_MAXSIZE 1024
/* log line maxsize */

char * islog_version(){
    return ISLOG_VERSION_NO;
}

/* 外部定义的 */
#include "wrktcp.h"
extern config cfg;

void islog_print(char *file, long line, char *level, char *fmtstr, ...)
{
    /* v1.1增加, test类型的话，只有istest模式才输出 */
    if( strcmp(level, "test") == 0){
        if( cfg.istest != 1)
            return;
    }
    va_list ap;
    char   tmpstr[ISLOG_LINE_MAXSIZE];

    memset(tmpstr, 0x00, sizeof(tmpstr));

    va_start(ap, fmtstr);
    vsnprintf(tmpstr, sizeof(tmpstr), fmtstr, ap);
    va_end(ap);

    fprintf(stdout, "[%s][%s][%03ld]", level, file, line);
    fprintf(stdout, "%s\n", tmpstr);

    return ;
}

