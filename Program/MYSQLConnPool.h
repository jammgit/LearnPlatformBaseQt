#ifndef MYSQLCONNPOOL_H
#define MYSQLCONNPOOL_H

#include <mysql.h>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <map>
#include <stdio.h>


/* 其实创建 和 销毁 由主线程完成就好了，就不用考虑那么多互斥安全的问题，其它操作才是给工作线程 
*  析构不知道怎么在多线程保证安全，程序员责任，我的脑袋要炸了 
*  销毁由一个线程完成，而创建和使用是线程安全的连接池 
*/
class MYSQLConnPool
{
public:
	enum ERRNO{
		E_MALLOC, 	/* 分配线程池时失败 */
		E_SUM_TO_MAX,	/* 连接池的连接数过大 */
		E_EXIST,		/* 连接池已存在 */
		E_NO_SOURCE,	/* 没有空闲连接 */
		E_INIT_SEM,	/* 初始化信号量错误（也许不该这样） */
		NONERR
	};

public:
	/* Singleton模式*/
	static MYSQLConnPool* CreateConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num)
	{
		pthread_mutex_lock(&m_MutexCreat);

		/* 连接数太大 */
		if (conn_num > MAX_CONN_NUM)
		{
			pthread_mutex_lock(&m_MutexMap);
			m_ErrMap[pthread_self()] = E_SUM_TO_MAX;
			pthread_mutex_unlock(&m_MutexMap);

			pthread_mutex_unlock(&m_MutexCreat);
			return nullptr;
		}
		if (m_ConnPool == nullptr)
		{
			m_ConnPool = new MYSQLConnPool(host, user, psw, db, conn_num);
			if (m_ErrMap[pthread_self()] == E_INIT_SEM)
			{/* 初始化信号量时出错 */
				delete m_ConnPool;
				pthread_mutex_unlock(&m_MutexCreat);
				return nullptr;
			}
			if (m_ConnPool)
			{/* 建立连接池成功 */
				pthread_mutex_unlock(&m_MutexCreat);
				return m_ConnPool;
			}
			/* new失败 */
			pthread_mutex_lock(&m_MutexMap);
			m_ErrMap[pthread_self()] = E_MALLOC;
			pthread_mutex_unlock(&m_MutexMap);

			pthread_mutex_unlock(&m_MutexCreat);
			return nullptr;
		}
		/* 该进程已经生成连接池 */
		pthread_mutex_lock(&m_MutexMap);
		m_ErrMap[pthread_self()] = E_EXIST;
		pthread_mutex_unlock(&m_MutexMap);

		pthread_mutex_unlock(&m_MutexCreat);
		return nullptr;

	}

	/*
	*	试想：一个监听主线程有几个服务线程，而每个服务线程（不同的服务封装了一个类）都想用到这个连接池（一个进程一个连接池），
	*	如果又上层（主线程）传到参数（连接池）到下层（服务线程）方式，那么太破坏服务类的封装了，故定义一个返回池指针的函数。
	*	（针对本开发的项目）
	*/
	static MYSQLConnPool* GetConnPoolPointer()
	{
		return m_ConnPool;
	}

	static enum ERRNO GetLastError()
	{
		pthread_mutex_lock(&m_MutexMap);
		std::map<pthread_t, ERRNO>::iterator ite;
		ite = m_ErrMap.find(pthread_self());
		if (ite == m_ErrMap.end())
		{
			pthread_mutex_unlock(&m_MutexMap);
			return NONERR;
		}
		enum ERRNO e = m_ErrMap[pthread_self()];
		pthread_mutex_unlock(&m_MutexMap);
		return e;
	}

	MYSQL *GetConnection()
	{/* 连接池没有空闲连接就阻塞 */	
		sem_wait(&m_ConnNum);
		pthread_mutex_lock(&m_MutexConnVec);
		MYSQL *conn = m_ConnVec.back();
		m_ConnVec.pop_back();
		printf("Distribute a connection\n");
		pthread_mutex_unlock(&m_MutexConnVec);
		return conn;
	}


	void RecoverConnection(MYSQL * &conn)
	{
		if (conn)
		{
			pthread_mutex_lock(&m_MutexConnVec);
			m_ConnVec.push_back(conn);
			printf("Recover a connection\n");
			pthread_mutex_unlock(&m_MutexConnVec);
			sem_post(&m_ConnNum);
			conn = nullptr;
		}
	}

	/* 目前没想到如何做到在多线程环境析构而绝对不冲突 */
	/* 虚函数不是原子操作，在多线程环境有可能在析构同时，另外一个线程在使用连接池 */
	~MYSQLConnPool()
	{
		/* 回收所有资源（连接）后，销毁信号量，就不可能再有线程非法操作了 */
		for (int i = 0; i < m_count; ++i)
		{/* 所以，使用了一定要归还啊，不然死的很惨+_+ */
			sem_wait(&m_ConnNum);
		}
		sem_destroy(&m_ConnNum);
		pthread_mutex_destroy(&m_MutexConnVec);
		for (int i = 0; i < m_count; ++i)
		{
			mysql_close(m_ConnVec[i]);
		}
		m_ConnPool = nullptr;
	}

private:
	MYSQLConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num);

private:
	/* 根据线程存储了已发生什么错误 */
	static pthread_mutex_t m_MutexMap;
	static std::map<pthread_t, enum ERRNO> m_ErrMap;

	/* 连接池最大连接数 */
	static const int MAX_CONN_NUM = 16;

	/* 连接池的连接数 */
	int m_count;
	/* 可用资源的信号量表示 */
	sem_t m_ConnNum;
	pthread_mutex_t m_MutexConnVec;

	/* 连接列表 */
	std::vector<MYSQL*> m_ConnVec;

	/* 多个线程一起创建时，使用互斥防止创建多个 */
	static pthread_mutex_t m_MutexCreat;

	/* 实例指针*/
	static MYSQLConnPool *m_ConnPool;
};

MYSQLConnPool* MYSQLConnPool::m_ConnPool = nullptr;

pthread_mutex_t MYSQLConnPool::m_MutexCreat = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t MYSQLConnPool::m_MutexMap = PTHREAD_MUTEX_INITIALIZER;

std::map<pthread_t, enum MYSQLConnPool::ERRNO> MYSQLConnPool::m_ErrMap;

MYSQLConnPool::MYSQLConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num)
{
	m_count = 0;
	for (int i = 0; i < conn_num; ++i)
	{
		MYSQL *conn = mysql_init(nullptr);
		if (!conn)
			continue;
		conn = mysql_real_connect(conn, host, user, psw, db, 0, nullptr, 0);
		if (!conn)
			continue;
		m_count++;
		m_ConnVec.push_back(conn);
	}
	/* 其实无关痛痒 */
	m_ErrMap[pthread_self()] = NONERR;
	int ret = sem_init(&m_ConnNum, 0, m_ConnVec.size());
	if (ret == -1)
	{
		pthread_mutex_lock(&m_MutexMap);
		m_ErrMap[pthread_self()] = E_INIT_SEM;
		pthread_mutex_unlock(&m_MutexMap);
	}
	m_MutexConnVec = PTHREAD_MUTEX_INITIALIZER;
}

#endif