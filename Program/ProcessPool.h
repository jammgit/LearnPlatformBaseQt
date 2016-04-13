
/*
*
*
*/

#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

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
#include <sys/stat.h>

#include "apue.h"

class Process
{
public:
	Process() : m_pid(-1) {}
public:
	pid_t m_pid;
	int m_pipefd[2];
};

template< typename T >
class ProcessPool
{
private:
	ProcessPool(int listenfd, int process_number = 4);

public:
	/* 保证创建唯一一个进程池 */
	static ProcessPool<T>* Create(int listenfd, int process_number = 4)
	{
		if (!m_instance)
		{
			m_instance = new ProcessPool<T>(listenfd, process_number);
		}
		return m_instance;
	}
	void Run();

private:
	void setup_sig_to_pipe();
	void run_parent();
	void run_child();

private:
	static const int MAX_PROCESS_NUMBER = 16;
	static const int USER_PER_PROCESS = 65536;
	static const int MAX_EVENT_NUMBER = 1024;

	int m_process_number;
	int m_index;
	int m_epollfd;
	int m_listenfd;
	bool m_stop;
	/* save sub process info*/
	Process *m_sub_process;
	static ProcessPool<T> *m_instance;
};

template<typename T>
ProcessPool<T>* ProcessPool<T>::m_instance = NULL;

template<typename T>
ProcessPool<T>::ProcessPool(int listenfd, int process_number)
	: m_listenfd(listenfd), m_process_number(process_number), m_index(-1),
	  m_stop(false)
{
	assert((process_number > 0) && (process_number < MAX_PROCESS_NUMBER));
	m_sub_process = new Process[process_number];
	assert(m_sub_process);

	/* 创建子进程，并且父子进程间通过unix域套接字通信 */
	for (int i = 0; i < process_number; ++i)
	{
		int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
		assert(ret == 0);
		m_sub_process[i].m_pid = fork();

		if (m_sub_process[i].m_pid == 0)
		{
			close(m_sub_process[i].m_pipefd[0]);
			m_index = i;
			break;
		}	
		else if (m_sub_process[i].m_pid > 0)
		{	
			close(m_sub_process[i].m_pipefd[1]);
		}	
	}

}


template<typename T>
void ProcessPool<T>::setup_sig_to_pipe()
{
	m_epollfd = epoll_create(256);
	assert(m_epollfd != -1);
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	assert(ret != -1);

	/* 对于父进程、子进程和所有其它进程，来自外界的信号都通过unix套接字传送到进程 */
	gSetNonblocking(sig_pipefd[1]);
	gAddfd(m_epollfd, sig_pipefd[0]);

	gAddSig(SIGCHLD, gSigHandler);
	gAddSig(SIGTERM, gSigHandler);
	gAddSig(SIGINT, gSigHandler);
	gAddSig(SIGPIPE, SIG_IGN);
}

template<typename T>
void ProcessPool<T>::Run()
{
	if (m_index != -1)
	{
		printf("child run\n");
		this -> run_child();
		
		return ;
	}
	printf("parent run\n");
	this -> run_parent();
	
}

template<typename T>
void ProcessPool<T>::run_child()
{
	this->setup_sig_to_pipe();

	/* 每个子进程都往epoll内核注册此套接字，当有新的连接时，父进程将以此套接字和子进程通信 */
	int pipefd = m_sub_process[m_index].m_pipefd[1];
	gAddfd(m_epollfd, pipefd);

	epoll_event event[MAX_EVENT_NUMBER];

	/* 服务类型的类，根据下文，T类必须实现init、process函数 */
	T *users = new T[USER_PER_PROCESS];
	assert(users);
	int number = 0;
	int ret = -1;

	while (!m_stop)
	{

		number = epoll_wait(m_epollfd, event, MAX_EVENT_NUMBER, -1);
		if (number < 0 && errno != EINTR)
		{
			printf("epoll failure\n");
			break;
		}	

		for (int i = 0; i < number; ++i)
		{
			int sockfd = event[i].data.fd;
			/* EPOLLIN事件来自父子进程通信的unix套接字，说明有新的用户连接服务器 */
			if (sockfd == pipefd && (event[i].events & EPOLLIN))
			{
				printf("child event come.\n");
				int client;
				ret = recv(sockfd, (char *)&client, sizeof(int), 0);
				if (ret < 0 && errno != EAGAIN || ret == 0)
					continue;
				struct sockaddr_in cliaddr;
				socklen_t clilen = sizeof(sockaddr_in);
				int connfd = accept(m_listenfd, (struct sockaddr *)&cliaddr, &clilen);
				printf("connect sock fd:%d\n", connfd);

				if (connfd < 0)
				{
					printf("errno is -> %d:%s\n", errno, strerror(errno));
					continue;
				}
				gAddfd(m_epollfd, connfd);
				users[connfd].init(m_epollfd, connfd, cliaddr);
				

			} /* 来子外界的信号，如在终端输入kill -signal PID给此进程时 */
			else if (sockfd == sig_pipefd[0] && (event[i].events & EPOLLIN))
			{
				int sig;
				int signals[256];
				ret = recv(sig_pipefd[0], (char *)signals, sizeof(signals), 0);
			
				if (ret <= 0)
					continue;
				
				for (int i = 0; i < ret; ++i)
				{
					switch (signals[i])
					{
						case SIGCHLD:
						{
							pid_t pid;
							int state;
							while ((pid = waitpid(-1, &state, WNOHANG)) > 0)
							{
								continue;
							}
							break;
						}
						case SIGTERM:
						case SIGINT:
						{
							m_stop = true;
							break;
						}
						default:
							break;
					}
				} 
			}/* 已经连接的用户发送请求的数据到达 */
			else if (event[i].events & EPOLLIN)
				users[sockfd].process();
			else
				continue;
		}	
	}
	delete [] users;
	users = NULL;
	close(pipefd);
	close(m_listenfd);
	close(m_epollfd);
	close(sig_pipefd[0]);   //close by create process
	close(sig_pipefd[1]);
	
}

template<typename T>
void ProcessPool<T>::run_parent()
{
	this->setup_sig_to_pipe();

	gAddfd(m_epollfd, m_listenfd);

	epoll_event event[MAX_PROCESS_NUMBER];
	int sub_process_counter = 0;
	int new_conn = 1;
	int number = 0;
	int ret = -1;

	while (!m_stop)
	{
		number = epoll_wait(m_epollfd, event, MAX_EVENT_NUMBER, 0);
		if (number < 0 && errno == EAGAIN)
		{
			printf("epoll_wait error -> %d:%s", errno, strerror(errno));
			break;
		}	

		for (int i = 0; i < number; ++i)
		{
			int sockfd = event[i].data.fd;
			/* 有新的连接，用轮询算法将accept等剩余工作递交给子进程 */
			if (sockfd == m_listenfd)
			{
				int idx = sub_process_counter;
				do
				{
					if (m_sub_process[idx].m_pid != -1)
						break;
					idx = (idx + 1)%m_process_number;
				}while (idx != sub_process_counter);

				if (m_sub_process[idx].m_pid == -1)
				{
					m_stop = true;
					break;
				}
				sub_process_counter = (sub_process_counter + 1)%m_process_number;
				printf("Send request to child [%d]\n", m_sub_process[idx].m_pid);
				send(m_sub_process[idx].m_pipefd[0], (char *)&new_conn, sizeof(new_conn), 0);
				

			} /* 有来自外界的信号 */
			else if (sockfd == sig_pipefd[0] && (event[i].events & EPOLLIN))
			{
				int sig;
				int signals[256];
				ret = recv(sockfd, (char *)&signals, sizeof(signals), 0);
				if (ret <= 0)
					continue;
				for (int i = 0; i < ret; ++i)
				{
					switch(signals[i])
					{
						case SIGCHLD:
						{
							pid_t pid;
							int state;
							while ((pid = waitpid(-1, &state, WNOHANG)) > 0)
							{
								for (int i = 0; i < m_process_number; ++i)
								{
									if (m_sub_process[i].m_pid == pid)
									{
										printf("child [%d] go die\n", pid);
										close(m_sub_process[i].m_pipefd[0]);
										m_sub_process[i].m_pid = -1;
									}
								}
							}
							m_stop = true;
							for (int i = 0; i < m_process_number; ++i)
							{
								if (m_sub_process[i].m_pid != -1)
									m_stop = false;
							}
							break;
						}
						case SIGINT:
						case SIGTERM:
						{
							printf("kill all child now\n");
							for (int i = 0; i < m_process_number; ++i)
							{
								if (m_sub_process[i].m_pid != -1)
								{
									kill(m_sub_process[i].m_pid, SIGTERM);
									close(m_sub_process[i].m_pipefd[0]);
								}
							}
							break;
						}
						default:
							break;
					}
				}
			}else 
				continue;

		}	
	}
	close(m_listenfd);
	close(m_epollfd);
	close(sig_pipefd[0]);
	close(sig_pipefd[1]);
	
}




#endif
