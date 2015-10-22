/*
 * http_server.c
 *
 *  Created on: 2015-1-7
 *      Author: root
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "http_server.h"

#define WEB_ROOT "/WEB"
int shmid = 0;

/*
 * 1.处理僵尸问题 signal
 * 2.处理超时问题(20) select alarm
 * 3.保存连接数目(统计在线连接数) shared memory
 */
//standard IO(FILE*)
//hello\r\n(\r 13 \n 10)
//deal line read
//GET / HTTP/1.1
int read_line(int fd,char *buf,int size)
{
	int i = 0;
	char ch;
	for(i = 0;i < size;++i)
	{
		int n = recv(fd,&ch,1,0);
		if(1 == n)
		{
			buf[i] = ch;
			//printf("%d\n",ch);
			if(ch == '\n') break;
		}
		else
		{
			return -1;
		}
	}
	return i+1;
}

void do_get(int fd,char *fileName)
{
	char path[128] = {'\0'};
	if(strcmp(fileName,"/") == 0) fileName = "/baidu.html";
	sprintf(path,"%s%s",WEB_ROOT,fileName);//衔接字符串
	FILE *fp = fopen(path,"r");
	if(fp == NULL)
	{
		printf("%s is not found\n",path);
		char errmsg[32] = {"<h1>404 Not Found</h1>"};
		send(fd,errmsg,strlen(errmsg),0);
		return ;
	}
	while(1)
	{
		char buf[1024] = {'\0'};
		int read_size = fread(buf,1,sizeof(buf),fp);
		if(read_size <= 0) break;
		send(fd,buf,read_size,0);
	}
	fclose(fp);
}

char* get_fileName(char *buf)
{
	char *p = (char *)malloc(256);
	p = strchr(buf,' ');
	if(p != NULL)
	{
		*p++ = '\0';
		if(strcmp(buf,"GET") == 0)
		{
			char *q = strchr(p,' ');
			if(q != NULL)
			{
				*q = '\0';
				return p;
			}
		}
		return NULL;
	}
	return NULL;
}

void data_process(int clientfd)
{
	fd_set rfds;//deal time out
	struct timeval tv;
	FD_ZERO(&rfds);
	FD_SET(clientfd, &rfds);
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	struct sockaddr_in client_addr;
	socklen_t c_length = sizeof(client_addr);
	getpeername(clientfd,(struct sockaddr *)(&client_addr),&c_length);
	printf("a client %s:%u connect!\n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);
//	addr = (int *)shmat(shmid,NULL,0);
//	*addr = 0;
//	*addr = *addr + 1;
//	printf("count = %d\n",*addr);
//	shmdt(addr);
	int ret_sl = select(clientfd+1,&rfds,NULL,NULL,&tv);
	if(ret_sl == 0)
	{
		printf("time out a client %s:%u disconnect!\n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);
		//close(clientfd);
//		addr = (int *)shmat(shmid,NULL,0);
//		*addr = *addr - 1;
//		printf("count = %d\n",*addr);
//		shmctl(shmid,IPC_RMID,NULL);
		exit(0);
	}
	while(1)
	{
		char buf[256] = {'\0'};
		int recv_size = read_line(clientfd,buf,sizeof(buf));//<==>read
		//printf("size:%d,recv:%s\n",recv_size,buf);
		if(recv_size <= 2) break;
		//printf("buf = %s\n",buf);

		char *fileName = get_fileName(buf);
		//printf("fileName = %s\n",fileName);
		if(fileName != NULL)
		{
			do_get(clientfd,fileName);
		}
	}
	printf("a client %s:%u disconnect!\n",inet_ntoa(client_addr.sin_addr),client_addr.sin_port);
	//close(clientfd);
//	addr = (int *)shmat(shmid,NULL,0);
//	*addr = *addr - 1;
//	printf("count = %d\n",*addr);
//	shmctl(shmid,IPC_RMID,NULL);
	exit(0);
}

int init_socket(int port)
{
	int socketfd = socket(AF_INET,SOCK_STREAM,0);
	if(socketfd == -1)
	{
		perror("socket error");
		exit(1);
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	memset(server_addr.sin_zero,0,sizeof(server_addr.sin_zero));
	int opt = 1;//1代表重用地址
	/* 如果服务器终止后,服务器可以第二次快速启动而不用等待一段时间  */
	int err = setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(int));
	if(err == -1)
	{
		perror("setsockopt error");
		exit(1);
	}
	int ret = bind(socketfd,(struct sockaddr *)(&server_addr),sizeof(server_addr));
	if(ret == -1)
	{
		perror("bind error");
		exit(1);
	}

	ret = listen(socketfd,20);
	if(ret == -1)
	{
		perror("listen error");
		exit(1);
	}
	printf("My Web Server is Ready!\n");
	return socketfd;
}

void start_server(int port)
{
	int socketfd = init_socket(port);
	while(1)
	{
		int clientfd = accept(socketfd,NULL,NULL);
		if(clientfd == -1)
		{
			perror("accept error");
			continue;
		}
		signal(SIGCHLD,sighandler);
//		shmid = shmget(1234,4,IPC_CREAT|0666);
//		printf("shmid = %d\n",shmid);
		//printf("g_data=%d\n",g_data);
		if(shmid == -1)
		{
			perror("error");
			exit(1);
		}
		int pid = fork();
		if(pid < 0)
		{
			perror("fork error");
			break;
		}
		else if(pid == 0)
		{
			data_process(clientfd);
		}
		close(clientfd);
	}
    close(socketfd);
}

void sighandler(int signo)
{
	printf("catch signo = %d\n",signo);
	while(1)
	{
		int ret= waitpid(-1,NULL,WNOHANG);
		printf("ret = %d\n",ret);
		if(ret > 0) return;
	}
}

