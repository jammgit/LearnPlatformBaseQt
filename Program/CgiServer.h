/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：简单回射服务
*/

#ifndef CGISERVER_H
#define CGISERVER_H
#include "apue.h"
 
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/stat.h>

#define MAX_RECV_BUF 512

class CgiServer
{
private:
	const int BUFFSIZE = 256;
	int m_epollfd;
	int m_connfd;
	struct sockaddr_in m_client;
public:
	CgiServer(int connfd):m_connfd(connfd) {};

	void Init(int epollfd, int connfd, const sockaddr_in& client)
	{
		m_epollfd = epollfd;
		m_connfd = connfd;
		m_client = client;
	}

	/* 进程将剩余任务交给子进程，然后自己立刻回到epoll_wait状态 */
	int Process()
	{
		int nRead;
		char buf[BUFFSIZE];
		char sendbuf[BUFFSIZE + 32];
		while (1)
		{
			memset(buf, 0, sizeof(buf));
			nRead = recv(m_connfd, buf, BUFFSIZE, 0);
			if (nRead == 0)
			{
				close(m_connfd);
				return -1;
			}
			else if (nRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			{
				return m_connfd;
			}
			/* 处理事件 */
			
			buf[nRead] = '\0';
			memset(sendbuf, 0, sizeof(sendbuf));
			sprintf(sendbuf, "You have send to server:%s", buf);
			send(m_connfd, buf, strlen(buf), 0);
		}

	}

};


#endif