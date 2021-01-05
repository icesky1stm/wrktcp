//
// Created by suitm on 2020/12/15.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "islog.h"

#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* 最大输出行数 */
#define ISLOG_LINE_MAXSIZE 1024
/* log line maxsize */

char * islog_version(){
    return ISLOG_VERSION_NO;
}

#if 0
/* REDIS的serverlog, TODO待修改 */
void serverLogRaw(int level, const char *msg) {
    const int syslogLevelMap[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING };
    const char *c = ".-*#";
    FILE *fp;
    char buf[64];
    int rawmode = (level & LL_RAW);
    int log_to_stdout = server.logfile[0] == '\0';

    level &= 0xff; /* clear flags */
    if (level < server.verbosity) return;

    fp = log_to_stdout ? stdout : fopen(server.logfile,"a");
    if (!fp) return;

    if (rawmode) {
        fprintf(fp,"%s",msg);
    } else {
        int off;
        struct timeval tv;
        int role_char;
        pid_t pid = getpid();

        gettimeofday(&tv,NULL);
        struct tm tm;
        nolocks_localtime(&tm,tv.tv_sec,server.timezone,server.daylight_active);
        off = strftime(buf,sizeof(buf),"%d %b %Y %H:%M:%S.",&tm);
        snprintf(buf+off,sizeof(buf)-off,"%03d",(int)tv.tv_usec/1000);
        if (server.sentinel_mode) {
            role_char = 'X'; /* Sentinel. */
        } else if (pid != server.pid) {
            role_char = 'C'; /* RDB / AOF writing child. */
        } else {
            role_char = (server.masterhost ? 'S':'M'); /* Slave or Master. */
        }
        fprintf(fp,"%d:%c %s %c %s\n",
                (int)getpid(),role_char, buf,c[level],msg);
    }
    fflush(fp);

    if (!log_to_stdout) fclose(fp);
    if (server.syslog_enabled) syslog(syslogLevelMap[level], "%s", msg);
}
#endif

void islog_print(char *file, long line, char *level, char *fmtstr, ...)
{
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

