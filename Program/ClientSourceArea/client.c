/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：服务器测试程序
*/

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/un.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "MsgStruct.h"
#include "MsgType.h"

//#define path "tempfile.socket"

void sig(int sig)
{
	char buf[512];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "write error, ertrno(%d) -> %s\n", errno, strerror(errno));
	write(STDERR_FILENO, buf, strlen(buf));
	sleep(3);
	exit(-1);
		
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage:./exec [port]\n");
		exit(-1);
	}

	char buf[512];
	memset(buf, 0, sizeof(buf));
	short type;
	int which;
	printf("Please input type:1[echo] 2[download file]");
	scanf("%d", &which);
	switch(which)
	{
		case 1:
			type = MSG_ECHO;
			strcpy(buf, "Please input string:");
			break;
		case 2:
			type = MSG_DOWNLOAD_FILE;
			strcpy(buf, "Please input filename:");
			break;
		default:
			break;
	}
	fflush(stdout);


	signal(SIGPIPE, sig);
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = inet_addr("192.168.1.115");

	connect(sock, (struct sockaddr *)&addr, sizeof(addr));

	int old = fcntl(sock, F_GETFL);
	int newoption = old | O_NONBLOCK;
	fcntl(sock, F_SETFL, newoption);



	int epollfd = epoll_create(5);
	epoll_event event[128];
	epoll_event e1,e2;
	e1.data.fd = sock;
	e1.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &e1); 
	e2.data.fd = STDIN_FILENO;
	e2.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &e2); 


	int nR;
	int ret;
	msgPack msg;
	int fd;
	bool isp = true;

	while (1)
	{
		if (isp)
		{
			printf("%s", buf);
			fflush(stdout);
		}

		int number = epoll_wait(epollfd, event, 128, -1);
		if (number <= 0)
		{
			printf("epoll_wait error\n");
			break;
		}

		for (int i = 0; i < number; ++i)
		{
			if (event[i].data.fd == sock && (event[i].events & EPOLLIN))
			{
				memset(&msg, 0, sizeof(msg));
				nR = read(sock, (char *)&msg, sizeof(msg));
				if (nR == 0)
				{
					close(sock);
					break;
				}

				if (ntohs(msg.type) == MSG_DOWNLOAD_FILE)
				{
					fd = open("file", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
					write(fd, msg.text, strlen(msg.text));
					close(fd);
					close(sock);
					exit(0);
				}

				write(STDOUT_FILENO, msg.text, strlen(msg.text));
				isp = true;
			}
			else if (event[i].data.fd == STDIN_FILENO && (event[i].events & EPOLLIN))
			{
				msg.type = htons(type);
				memset(msg.text, 0, sizeof(msg.text));
				nR = read(STDIN_FILENO, msg.text, TEXTSIZE);
				// 去掉换行符
				msg.text[nR - 1] = '\0';
									
				ret = write(sock, (char *)&msg, sizeof(msg.type) + strlen(msg.text));
				if (ret < 0)
				{
					close(sock);
					printf("write sock ret < 0\n");
					exit(-1);
				}
				isp = false;
			}
		}
	}

	close(sock);

	return 0;
}
