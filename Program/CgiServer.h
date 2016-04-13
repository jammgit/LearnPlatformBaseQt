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
	int m_epollfd;
	int m_sockfd;
	struct sockaddr_in m_client;
public:
	

	void init(int epollfd, int sockfd, const sockaddr_in& client)
	{
		m_epollfd = epollfd;
		m_sockfd = sockfd;
		m_client = client;
	}

	/* 进程将剩余任务交给子进程，然后自己立刻回到epoll_wait状态 */
	void process()
	{
		pid_t pid;
		int nRead;
		char buf[MAX_RECV_BUF];
		char replybuf[MAX_RECV_BUF + 64];
		if ((pid = fork()) < 0)
		{
			// how to process it?
		}
		else if (pid == 0)
		{
			/* 在gAddfd时，套接字被设置为非阻塞的，如果不设置为阻塞，那么在下面while循环将出现问题 */
			int old_option = fcntl(m_sockfd, F_GETFL);
			int new_option = old_option & ~O_NONBLOCK;
			fcntl(m_sockfd, F_SETFL, new_option);

			while (true)
			{
				/* 简单的应答测试 */
				memset(buf, 0, sizeof(buf));
				nRead = read(m_sockfd, buf, MAX_RECV_BUF);
				if (nRead == 0)
				{
					close(m_sockfd);
					exit(-1);
				}
				buf[nRead] = '\0';
				memset(replybuf, 0, sizeof(replybuf));
				sprintf(replybuf, "You have send to server:%s", buf);
				write(m_sockfd, replybuf, strlen(replybuf));
			
			}
		}
		/* 父进程不再监听此套接字 */
		gRemovefd(m_epollfd, m_sockfd);

	}

};


#endif