/************************************
 * istcp.c is a tcp protrol library
 * author: icesky
 * date: 2020.12.21
 *************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
/* af_unix,域socket使用 */
#include <sys/un.h>

#include "istcp.h"


/*** 关闭socket ***/
void istcp_close(int sock){
   close(sock);
   return;
}

/*** 申请socket并连接 ***/
int istcp_connect(char *ip,  int port){
    struct sockaddr_in sin;
    int sock;

    memset(&sin, 0x00, sizeof(sin));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(ip);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        return ISTCP_ERROR_SOCKET;
    }

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        close(sock);
        return ISTCP_ERROR_CONNECT;
    }

    return sock;
}

/*** unix域socket ***/
int istcp_connect_unix(char *pathname)
{
    int sock;
    struct sockaddr_un sun;

    memset(&sun, 0x00, sizeof(sun));

    sun.sun_family = AF_UNIX;
    if( strlen( pathname) >= sizeof(sun.sun_path)){
        return ISTCP_ERROR_UNIXPATH_TOOLONG;
    };
    strcpy (sun.sun_path, pathname);

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        return ISTCP_ERROR_SOCKET;
    }

    if (connect(sock, (struct sockaddr *)&sun, sizeof(sun)) < 0){
        close(sock);
        return ISTCP_ERROR_CONNECT;
    }

    return sock;

}

/*** 按超时时间接收固定长度内容  ***/
static int istcp_timeflag = 0 ;
static void istcpSetalm(int timeout) {
    alarm(timeout) ;
    return ;
}
static void istcpClralm(){
    alarm(0) ;
    return ;
}

static void istcpCthalm(int a){
    istcp_timeflag = 1 ;
    return ;
}

int istcp_recv(int sock, char *msgbuf, int len, int timeout){
    int ret;
    int nfds;
    struct timeval tv;
    fd_set readfds;

    int iSize;
    long lStartTime, lCurrentTime;

    lStartTime = lCurrentTime = 0;

    /* 设置超时时间 */
    if (timeout <= 0){
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
    } else {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    /* 设置nfds = 1， 检查当前sock */
    nfds = sock + 1;

    /* 初始化readfds, 并将当前sock加入 */
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    time(&lStartTime);

    ret = select(nfds, &readfds, NULL, NULL, &tv);
    if (ret < 0){
        return ISTCP_ERROR_SELECT;
    }else if (ret == 0){
        /* 超时时间到 */
        return ISTCP_ERROR_TIMEOUT;
    }


    /* 异常错误 */
    if (!FD_ISSET(sock, &readfds)){
        return ISTCP_ERROR_SELECT;
    }

    /* 开始接收数据包 */
    iSize = len ;
    while(iSize > 0){
        if (timeout > 0){
            time(&lCurrentTime);
            if (lCurrentTime - lStartTime > timeout){
                return ISTCP_ERROR_TIMEOUT;
            }
        }

        signal(SIGALRM, istcpCthalm) ;
        istcpSetalm(timeout) ;

        ret = read(sock, msgbuf, iSize);

        istcpClralm();

        if ( istcp_timeflag == 1 ){
            istcp_timeflag = 0 ;
            return ISTCP_ERROR_TIMEOUT;
        }

        if (ret < 0){
            return ISTCP_ERROR_READ;
        } else if (ret == 0){
            return ISTCP_ERROR_DISCONNECT_PEER;
        } else {
            iSize -= ret;
            msgbuf += ret;
        }
    }
    return 0;
}

/*** 按超时时间等待,试图读一次接收固定长度内容，返回实际读取长度,不可靠的通讯模式，仅用于对方无法约定报文长度时使用 ***/
int istcp_recv_nowait(int sock, char *msgbuf, int len, int timeout){
    int ret;
    int nfds;
    struct timeval tv;
    fd_set readfds;

    int iSize;
    long lStartTime, lCurrentTime;

    lStartTime = lCurrentTime = 0;

    /* 设置超时时间 */
    if (timeout <= 0){
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
    } else {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    /* 设置nfds = 1， 检查当前sock */
    nfds = sock + 1;

    /* 初始化readfds, 并将当前sock加入 */
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    time(&lStartTime);

    ret = select(nfds, &readfds, NULL, NULL, &tv);
    if (ret < 0){
        return ISTCP_ERROR_SELECT;
    }
    else if (ret == 0){
        /* 超时时间到 */
        return ISTCP_ERROR_TIMEOUT;
    }

    /* 异常错误 */
    if (!FD_ISSET(sock, &readfds)){
        return ISTCP_ERROR_SELECT;
    }

    /* 开始接收数据包 */
    iSize = len;
    if (timeout > 0){
        time(&lCurrentTime);
        if (lCurrentTime - lStartTime > timeout){
            return ISTCP_ERROR_TIMEOUT;
        }
    }

    ret = read(sock, msgbuf, iSize);
    if (ret < 0) {
        return ISTCP_ERROR_READ;
    }
    else if (ret == 0){
        return ISTCP_ERROR_DISCONNECT_PEER;
    }

    return ret;
}


/*** 按长度发送内容 ***/
int istcp_send(int sock, char *msgbuf, int len, int timeout){
    int    ret;
    int    nfds;
    struct timeval tv;
    fd_set writefds;
    long   lStartTime, lCurrentTime;
    int    iSize;

    lStartTime = lCurrentTime = 0;

    /* 设置超时时间 */
    if (timeout <= 0){
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
    }else{
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    time(&lStartTime);

    /* 设置nfds = 1, 检查当前sock */
    nfds = sock + 1;

    /* 初始化writefds, 并将当前sock加入 */
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    ret = select(nfds, NULL, &writefds, NULL, &tv);
    if (ret < 0){
        return ISTCP_ERROR_SELECT;
    }else if (ret == 0){
        /* 超时时间到 */
        return ISTCP_ERROR_TIMEOUT;
    }

    /* 异常错误 */
    if (!FD_ISSET(sock, &writefds)){
        return ISTCP_ERROR_SELECT;
    }

    iSize = len;
    while(iSize > 0){
        if (timeout > 0){
            time(&lCurrentTime);
            if (lCurrentTime - lStartTime > timeout){
                return ISTCP_ERROR_TIMEOUT;
            }
        }

        ret = write(sock, msgbuf, iSize);
        if (ret <= 0){
            return ISTCP_ERROR_WRITE;
        }else{
            iSize -= ret;
            msgbuf += ret;
        }
    }

    return 0;

}

/** accept获取新连接 **/
int istcp_accept( int sock){
    return istcp_accept_gethost( sock, NULL);
}

/** 获取新连接并取得请求信息 **/
int istcp_accept_gethost(int sock, char * *p_hostip) {
    int acceptSock;
    socklen_t len = 0;
    struct sockaddr_in sin;

    len = sizeof(struct sockaddr_in);
    acceptSock = accept(sock, (struct sockaddr *) &sin, &len);
    if (acceptSock < 0){
        return ISTCP_ERROR_ACCEPT;
    }
    /* 如果不需要，则直接赋值 */
    if( p_hostip != NULL){
        *p_hostip = inet_ntoa(sin.sin_addr);
    }

    return acceptSock;
}

/** 带backlog队列大小的绑定端口和监听 **/
int istcp_listen_backlog(char *hostname, int port, int backlog){
    struct hostent *h;
    struct sockaddr_in sin;
    int sock;
    int optLen, optVar;
    int ret;

    int ip1,ip2,ip3,ip4;

    memset(&sin, 0x00, sizeof(sin));

    if (hostname != NULL) {
        /** 如果是ip地址类型，则直接赋值 **/
        if( 4 == sscanf(hostname, "%d.%d.%d.%d",&ip1, &ip2, &ip3, &ip4)){
            if (0<=ip1 && ip1<=255
                && 0<=ip2 && ip2<=255
                && 0<=ip3 && ip3<=255
                && 0<=ip4 && ip4<=255){
                sin.sin_addr.s_addr = inet_addr(hostname);
            }
        } else {
            /** 如果不是ip地址类型，则取DNS信息 **/
            h = gethostbyname(hostname);
            if (h == NULL){
                sin.sin_addr.s_addr = inet_addr(hostname);
            } else {
                memcpy(&sin.sin_addr, h->h_addr, h->h_length);
            }
        }
    }else{
        sin.sin_addr.s_addr = INADDR_ANY; /*本机IP*/
    }

    sin.sin_port = htons(port);
    sin.sin_family = AF_INET;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
        return ISTCP_ERROR_SOCKET;
    }

    optVar = 1;
    optLen = sizeof(optVar);

    ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optVar, optLen);
    if (ret < 0){
        return ISTCP_ERROR_SETSOCKOPT;
    }

    if (bind(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr)) < 0){
        close(sock);
        return ISTCP_ERROR_BIND;
    }

    if (listen(sock, backlog) < 0){
        close(sock);
        return ISTCP_ERROR_LISTEN;
    }

    return sock;
}

/** 标准绑定监听服务 **/
int istcp_listen(char *hostname, int port){
    return istcp_listen_backlog( hostname, port, 10);
}

