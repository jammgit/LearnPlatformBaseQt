#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "MsgType.h"
#include "MsgStruct.h"

int gSetNonblock(int fd)
{
	int old = fcntl(fd, F_GETFL);
	int newoption = old | O_NONBLOCK;
	fcntl(fd, F_SETFL, newoption);
	return old;
}

void gAddfd(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLOUT | EPOLLET | EPOLLERR;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	gSetNonblock(fd);
}

bool gWriteN(int sockfd, char *data, int len)
{
	int nW;
	int dlen = len;
	while (1)
	{
		nW = write(sockfd, data, len);
		if (nW == 0 || nW == -1)
			return false;
		len -= nW;
		data += nW;
		if (len <= 0)
		{
			printf("Send sockfd[%d] %d bytes data\n", sockfd, dlen);
			return true;
		}
	}
}

bool gReadN(int sockfd, char *data, int len)
{
	int nR;
	nR = read(sockfd, data, len);
	if (nR == 0 || nR == -1)
		return false;
	printf("Recv sockfd[%d] %d bytes data\n", sockfd, nR);
	return true;
}

void gConnectN(int epollfd, int num, char *ip, short port)
{
	int ret = 0;
	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);
	serv.sin_addr.s_addr = inet_addr(ip);

	for (int i = 0; i < num; ++i)
	{
		sleep(1);
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			continue;
		if (connect(sockfd, (struct sockaddr *)&serv, sizeof(serv)) == 0)
		{
			printf("Build a connect to [%s:%d]\n", ip, port);
			gAddfd(epollfd, sockfd);
		}
	}
}

void gClose(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printf("Usage:./exec [ip] [port] [conn_num]\n");
		return 0;
	}

	int epollfd = epoll_create(1024);
	gConnectN(epollfd, atoi(argv[3]), argv[1], atoi(argv[2]));
	epoll_event event[10000];
	msgPack msg;
	int number;
	while (1)
	{
		number = epoll_wait(epollfd, event, 10000, 2000);
		for (int i = 0; i < number; ++i)
		{
			int sockfd = event[i].data.fd;
			if (event[i].events & EPOLLIN)
			{
				if (! gReadN(sockfd, (char *)&msg, sizeof(msg)))
					gClose(epollfd, sockfd);
				epoll_event e;
				e.events = EPOLLOUT | EPOLLET | EPOLLERR;
				e.data.fd = sockfd;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &e);
			}
			else if (event[i].events & EPOLLOUT)
			{
				memset(&msg, 0, sizeof(msg));
				msg.type = htons(MSG_ECHO);
				strncpy(msg.text, "Hello server\n", 14);
				if (! gWriteN(sockfd, (char *)&msg, sizeof(msg.type) + strlen(msg.text)))
					gClose(epollfd, sockfd);
				epoll_event e;
				e.events = EPOLLIN | EPOLLET | EPOLLERR;
				e.data.fd = sockfd;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &e);
			}
			else if (event[i].events & EPOLLERR)
			{
				gClose(epollfd, sockfd);
			}
		}
	}
}
