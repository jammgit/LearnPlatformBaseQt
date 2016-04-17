/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：此文件封装了一个线程类，主要工作为创建线程池、添加任务到列表、处理任务。
*		 注意，模板参数的类必须实现了公有函数void Process()，用处处理指定实务
*		 ；如用作http时，http应答的处理应该在此函数实现。
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <list>
#include "apue.h"

template<typename T>
class ThreadPool
{

public:
	/*	2016.02.17/17:13！
	*	由于对每个链接套接字设置了EPOLLONESHOT，故需要将进程epollfd传递进来，待线程处理完上次请求后，
	*	再向epollfd注册该套接字。（此函数委托默认构造函数完成一部分构造/C++类型转换）
	*/
	ThreadPool(int epollfd, long threadnum = 8, long maxreqnum = 1000)
		: ThreadPool(threadnum, maxreqnum)
	{
		m_epollfd = epollfd;
	};
	/*	为提高线程池类的复用性，提供一个默认构造函数。
	*/
	ThreadPool(long threadnum = 8, long maxreqnum = 1000);

	~ThreadPool()
	{
		delete [] m_Pthread;
		m_Stop = true;
	}
	/* 往队列添加任务 */
	bool Append(T *request);

	/*	线程函数不能执行对象成员函数，所以声明为static，此时函数不能使用对象非静态成员，故将访问的工作交给Run函数，
	*   this 传递到args。（Work应该封装到类里面）——Singleton模式 
	*/
	static void* Work(void *args);
	void Run(int index);

private:
	/* 进程级epoll事件描述符，当使用默认构造函数构造的对象时，该变量无用 */
	int m_epollfd;

	/* 请求队列最大任务数 */
	int m_MaxRequestNum;
	/* 线程池线程数量 */
	int m_ThreadNum;

	pthread_t *m_Pthread;
	/* 线程是否应该结束 */
	bool m_Stop;

	/* 子线程在等待任务时，应该进入睡眠，用信号量；而资源竞争（获取队列中的一个任务）用互斥量保护 */
	pthread_mutex_t m_QueMutex;
	sem_t m_QueSem;

	/* 任务列表 */
	std::list<T *> m_WorkList;
};

template<typename T>		
ThreadPool<T>::ThreadPool(long threadnum, long maxreqnum)
	: m_MaxRequestNum(maxreqnum), m_ThreadNum(threadnum),
	  m_Pthread(nullptr), m_Stop(false), m_epollfd(-1)
{
	int ret;
	/* 信号量值小于零就block */
	ret = sem_init(&m_QueSem, 0, 0);
	if (ret == -1)
	{
		printf("ThreadPool: [sem_init] failed, errno[%d]:%s\n", errno, strerror(errno));
		exit(-1);
	}
	m_QueMutex = PTHREAD_MUTEX_INITIALIZER;

	if (!m_Pthread)
	{
		m_Pthread = (pthread_t *)malloc(sizeof(pthread_t) * m_ThreadNum);
		/* 如果分配失败，应该退出 */
		if (!m_Pthread)
		{
			printf("ThreadPool: [malloc] failed, errno[%d]:%s\n", errno, strerror(errno));
			exit(-1);
		}
	}

	for (int i = 0; i < threadnum; ++i)
	{
		/* 将this指针传递给static成员函数，实现Singleton模式 */
		ret = pthread_create(m_Pthread + i, nullptr, Work, this);
		if (ret != 0)
		{
			delete [] m_Pthread;
			printf("ThreadPool: [pthread_create] failed, errno[%d]:%s\n", ret, strerror(ret));
			exit(-1);
		}
		/* 分离线程，子线程马上到执行区等待任务 */
		ret = pthread_detach(m_Pthread[i]);
		if (ret != 0)
		{
			delete [] m_Pthread;
			printf("ThreadPool: [pthread_detach] failed, errno[%d]:%s\n", ret, strerror(ret));
			exit(-1);			
		}
	}
}

template<typename T>
bool ThreadPool<T>::Append(T *request)
{
	pthread_mutex_lock(&m_QueMutex);
	
	if (m_WorkList.size() > m_MaxRequestNum)
	{
		pthread_mutex_unlock(&m_QueMutex);
		printf("ThreadPool: [Append] failed, Client sum is over m_MaxRequestNum \n");
		return false;
	}
	m_WorkList.push_back(request);

	pthread_mutex_unlock(&m_QueMutex);
	sem_post(&m_QueSem);
	return true;
}

template<typename T>
void* ThreadPool<T>::Work(void *args)
{
	static int index = 0;
	index++;
	ThreadPool<T> *pthreadpool = (ThreadPool<T> *)args;
	pthreadpool->Run(index);
	return nullptr;
}

template<typename T>
void ThreadPool<T>::Run(int index)
{
	while (!m_Stop)
	{

		sem_wait(&m_QueSem);
		printf("pid:%d pthread:%d\n", getpid(), index);
		pthread_mutex_lock(&m_QueMutex);
		
		if (m_WorkList.empty())
		{
			pthread_mutex_unlock(&m_QueMutex);
			continue;
		}
		/* 获取一个任务 */
		T* request = m_WorkList.front();
		m_WorkList.pop_front();

		pthread_mutex_unlock(&m_QueMutex);
		if (!request)
		{
			continue;
		}
		/* 返回该线程处理的套接字 */
		int sockfd = request->Process();
		/* 如果该线程的套接字设置了EPOLLONESHOT;另外，如果Process返回小于零值，说明套接字被关闭，此时不应该再注册该套接字 */
		if (m_epollfd > 0 && sockfd > 0)
		{
			gResetOneshot(m_epollfd, sockfd);
		}
		delete request;
	}
}




#endif