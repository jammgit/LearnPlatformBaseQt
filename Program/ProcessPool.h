
/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：此文件封装了一个进程程类，主要工作为检测监听/连接套接字，将消息传达给相
*		 应服务。注意，模板参数的类必须实现了公有函数void Process()，用处处理
*		 指定实务；如用作http时，http应答的处理应该在此函数实现。
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

#include "MYSQLConnPool.h"
/*
*	ThreadPool.h 和 apue.h包含的顺序不能变；若倒置，由于ProcessPool.h包含ThreadPool.h，而
*	ProcessPool.h已包含apue.h，所以在ThreadPool.h中就算有#include"apue.h"的语句，它也不会
*	包含它进去，故编译时会找不到函数地址，在连接阶段就出现：对‘gResetOneshot(int, int)’未定义的引用
*/
#include "ThreadPool.h"
/*	其实也不用再包含此头文件，因为ThreadPool.h 已包含 */
#include "Apue.h"

extern MYSQLConnPool *connpool;
/* 父进程用于标志子进程的类 */
class Process
{
public:
	Process() : m_pid(-1) {}
public:
	pid_t m_pid;
	int m_pipefd[2];
};

/* 进程池类 */ 
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
	/* 最大进程数量 */
	static const int MAX_PROCESS_NUMBER = 16;
	/* 每个进程最大epoll注册描述符数量 */
	static const int MAX_EVENT_NUMBER = 1024;

	/* 进程数量 */
	int m_process_number;
	/* 子进程索引 */
	int m_index;
	/* 进程epoll描述符*/
	int m_epollfd;
	/* 监听套接字 */
	int m_listenfd;

	/* 进程是否停止 */
	bool m_stop;

	/* 进程信息数组 */
	Process *m_sub_process;
	/* 进程池实例 */
	static ProcessPool<T> *m_instance;
};

template<typename T>
ProcessPool<T>* ProcessPool<T>::m_instance = nullptr;

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
	/* 内核2.6.8以上的linux系统由内核界定，否则参数没用（原指注册的最大描述符值） */
	m_epollfd = epoll_create(5);
	assert(m_epollfd != -1);
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	assert(ret != -1);

	/* 对于父进程、子进程和所有其它进程，来自外界的信号都通过unix套接字传送到进程 */
	gSetNonblocking(sig_pipefd[1]);
	gAddfd(m_epollfd, sig_pipefd[0]);
	/* 指定的信号都通过sig_pipefd[1]套接字传送给进程 */
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

	/* 一次从内核返回的任务最大值为MAX_EVENT_NUMBER */
	epoll_event event[MAX_EVENT_NUMBER];

	/* 实例化线程池 */
	ThreadPool<T> tpool(m_epollfd);
	connpool = MYSQLConnPool::CreateConnPool("localhost",
											 "root",
											 "root",
											 "LearnPlatformBaseQT",
											 4);	// 建立连接数量
	if (!connpool)
	{
		printf("connect mysql failed\n");
		exit(-1);
	}

	int number = 0; /* epoll_wait返回值存储变量 */
	int ret = -1;  /* 各种系统调用返回值存储变量 */

	while (!m_stop)
	{
		/* 如果设置非阻塞，那么cpu使用率很高，没必要设置非阻塞 */
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
				/* 设置连接套接字EPOLLONESHOT */
				gAddfd(m_epollfd, connfd, true);
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
			{
				//users[sockfd].Process();
				tpool.Append(new T(sockfd));
			}
			else
				continue;
		}	
	}
	close(pipefd);
	close(m_listenfd);
	close(m_epollfd);
	close(sig_pipefd[0]);   
	close(sig_pipefd[1]);
	delete connpool;
	connpool = nullptr;
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
		number = epoll_wait(m_epollfd, event, MAX_EVENT_NUMBER, -1);
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
