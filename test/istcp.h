//
// Created by icesky on 2020/12/21.
//

#ifndef ISTCP_H
#define ISTCP_H

#define ISTCP_VERSION_NO "20.12.3"
char * istcp_version();


#define ISTCP_ERROR_SOCKET -5
#define ISTCP_ERROR_CONNECT -10

#define ISTCP_ERROR_SELECT -15
#define ISTCP_ERROR_TIMEOUT -20
#define ISTCP_ERROR_DISCONNECT_PEER -25
#define ISTCP_ERROR_READ -30
#define ISTCP_ERROR_WRITE -40

#define ISTCP_ERROR_ACCEPT -50
#define ISTCP_ERROR_SETSOCKOPT -55
#define ISTCP_ERROR_BIND  -60
#define ISTCP_ERROR_LISTEN  65

#define ISTCP_ERROR_UNIXPATH_TOOLONG -105

int istcp_connect(char *ip,  int port);
int istcp_send(int sock, char *msgbuf, int len, int timeout);
int istcp_recv(int sock, char *msgbuf, int len, int timeout);
int istcp_recv_nowait(int sock, char *msgbuf, int len, int timeout);
void istcp_close(int sock);

int istcp_listen(int port);
int istcp_listen_backlog(char *hostname, int port, int backlog);

int istcp_accept(int sock);
int istcp_accept_gethost(int sock, char * *p_hostip);

int istcp_connect_unix(char *pathname);
int istcp_listen_unix(char *pathname);

#endif //ISTCP_H
