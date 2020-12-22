/************************************
* �������� icesky_rcv.c
* ��  ��:  suitm
* ��  �ܣ� AIX tcp ����ͨѶ
* ʱ  �䣺 2011.3.28
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

/***�ػ�����***/
int main(int argc, char *argv[])
{
	pid_t	pid ;
    /***��������***/
	if ( ( pid=fork() ) < 0  ) 
	{
		printf("fork pid error\n") ;
		return 1 ;
	}
	else if ( pid>0 )/*��ֹ������*/
	{
		exit(0) ;
	}
	if ( setsid() == -1) /*������һ���µĽ�����*/
	{
	   printf("setsid error\n") ;
	   return 1 ;
	}

	sigset(SIGHUP,SIG_IGN) ;/*�����û��˳�ʱ�ź�*SIG_DFL�ָ�*/
	sigset(SIGCHLD,SIG_IGN) ;/*�����ӽ��̽���ʱ�ź�*/
#if 0
	sigset(SIGUSR1,exit_server) ;/*�û��Զ����˳��źţ����˳�*/
#endif
	
	/** ������signal�����յ����ź��Ժ󣬻�SIG_DFL���ź�.sigset�򲻻�*/
	/*����ļ�����*/
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

/***  ����������� ***/
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

	/*�ж϶˿�Ҫ����1024*/
	for (i = 0; i < strlen(argv[1]); i++) 
	{
		if (!isdigit(argv[1][i])) /*�ַ��Ƿ�������*/
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

		if ( (child_pid = fork()) > 0 ) /*������������������������*/
		{
			close(newsock);/*�ر�newsock*/
			continue;
		} 
		else if ( child_pid == 0 )/*�ӽ���*/
		{
			/*��ʼ�����������̵ĵ���*/
			close(sock);/*�رո�����sock*/
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
