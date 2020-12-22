/************************************
* 程序名： icesky_rcv.c
* 作  者:  suitm
* 功  能： AIX tcp 接收通讯
* 时  间： 2011.3.28
*************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

#include "istcp.h"

#define USLEEP_TIME 10000

static int	icesky_rcv(int argc, char *argv[]);

void exit_server(int s)
{
  printf("\n### USR1 EXIT ###\n");

  exit(0);
}

/***守护进程***/
int main(int argc, char *argv[])
{
	pid_t	pid ;
    /***创建进程***/
	if ( ( pid=fork() ) < 0  ) 
	{
		printf("fork pid error\n") ;
		return 1 ;
	}
	else if ( pid>0 )/*终止父进程*/
	{
		exit(0) ;
	}
	if ( setsid() == -1) /*创建了一个新的进程组*/
	{
	   printf("setsid error\n") ;
	   return 1 ;
	}

	sigset(SIGHUP,SIG_IGN) ;/*忽略用户退出时信号*SIG_DFL恢复*/
	sigset(SIGCHLD,SIG_IGN) ;/*忽略子进程结束时信号*/
#if 0
	sigset(SIGUSR1,exit_server) ;/*用户自定义退出信号，则退出*/
#endif
	
	/** 可以用signal会在收到该信号以后，会SIG_DFL该信号.sigset则不会*/
	/*清除文件掩码*/
	umask( 0 );
	errno=0 ;

	if (icesky_rcv(argc,argv) )
	{
		printf("icesky_rcv error\n") ;
	}
	else
	{
		printf("icesky_rcv success\n") ;
    }
	return 0 ;
}

/***  监听处理程序 ***/
static int	icesky_rcv(int argc, char *argv[])
{
	int	newsock;
	int sock; 
	int i;
	unsigned int portnum;
	char msg[2560];
	char head[200];
	pid_t child_pid;

	char * hostip = "0000";
	int conn_num = 0;

	if (argc != 2) 
	{
		printf("\n usage: %s PORT\n\n", argv[0]);
		exit(-1);
	}

	/*判断端口要大于1024*/
	for (i = 0; i < strlen(argv[1]); i++) 
	{
		if (!isdigit(argv[1][i])) /*字符是否是数字*/
		{
			printf("\n usage: %s PORT\n\n", argv[0]);
			exit(-1);
		}
	}
	
	portnum = atoi(argv[1]);
	
	if (portnum <= 1024) 
	{
		printf("\nPORT [%s] less than 1024!\n", argv[1]);
		exit(-1);
	}
	if( (sock = istcp_listen_backlog(NULL, portnum, 10)) < 0){
        perror("socket");
        printf("--->%s,%d create sock error!!!", __FILE__, __LINE__);
        exit(1);
	}
	printf("--->Server[%d] StartUp Success !!!\n", getpid());

	while (1) 
	{
        conn_num++;
		if ( (newsock = istcp_accept_gethost(sock, &hostip)) < 0)
		{
            printf("icesky_rcv[%d],[%s][%d]\n", getpid(), __FILE__, __LINE__);
            return -5;
			continue;
		}

        /*
         * 
		printf("--->PARENT-ACCEPT FROM[%d][%s], num:[%d] !\n", newsock, hostip, conn_num);
        */

		if ( (child_pid = fork()) > 0 ) /*父进程跳出，继续接收请求*/
		{
			close(newsock);/*关闭newsock*/
			continue;
		} 
		else if ( child_pid == 0 )/*子进程*/
		{
			/*开始进行主控流程的调度*/
			close(sock);/*关闭父进程sock*/
			memset( head, 0x00, sizeof( head));
            memset( msg, 0x00, sizeof( msg));
			if ( istcp_recv( newsock, head, 8, 60) <0)
			{
			    printf( "[%d]recv head error!![%d]\n", conn_num, newsock);
			    exit(1);
			}
			if( head[0] == 0x00 || atol(head) <= 0 || atol(head) > 1000000) {
                printf( "[%d]recv head error!![%s]\n", conn_num, head);
                exit(1);
			}
            if ( istcp_recv( newsock, msg, atol(head), 60) <0)
            {
                printf( "[%d]recv msg error!![%d]", conn_num, newsock);
                exit(1);
            }
            printf( "NUM:[%d] RECV:[%s%s]\n", conn_num, head, msg);

			usleep(USLEEP_TIME);

			strcpy(msg,"00000013000000SUCEESS");
			if ( istcp_send( newsock, msg, strlen(msg), 60)<0)
			{
			    printf( "[%d]send msg error![%d]", conn_num, newsock);
			    exit(1);
			}
            /*
            printf( "[%d]SEND:[%s], [%d]\n", conn_num, msg, getpid());
            */

			close(newsock);
			exit(0);
		}
	}
    return 0;
}
