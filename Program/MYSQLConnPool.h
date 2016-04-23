#ifndef MYSQLCONNPOOL_H
#define MYSQLCONNPOOL_H

#include <mysql.h>
#include <semaphore.h>
#include <pthread.h>
#include <vector>

/*	原始mysql函数：
*	MYSQL *mysql_init(MYSQL *); // 初始化连接句柄  
*	MYSQL *mysql_real_connect(MYSQL *connection,   
*	                          const char *server_host,                          
*	                          const char *sql_user_name,  
*	                          const char *sql_password,   
*	                          const char *db_name,   
*	                          unsigned int port_number,   
*	                          const char *unix_socket_name,   
*	                          unsigned int flags); //建立连接  
*	void mysql_close(MYSQL *connection); //关闭连接  
*	int mysql_options(MYSQL *connection, enum option_to_set, const char *argument); //设置连接选项  
*	option_to_set的值为下列三个值之一：  
*	MySQL_OPT_CONNECT_TIMEOUT  //连接超时前的等待秒数  
*	MySQL_OPT_COMPRESS //网络连接中使用压缩机制  
*	MySQL_INIT_COMMAND //每次建立连接后发送的命令  
*
*	unsigned int mysql_errno(MYSQL *connection); //返回错误代码(非零值)  
*	char *mysql_error(MYSQL *connection); //返回错误信息  
*	  
*	int mysql_query(MYSQL *connection, const char *query); //执行SQL语句，成功返回0
*
*/

class MYSQLConnPool
{
public:
	typedef struct 
	{
		bool flag;   /* 判断此连接是否已经被占用（true） */
		MYSQL *conn;
	}SMYSQL;
public:
	/* Singleton模式 */
	static MYSQLConnPool* CreateConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num)
	{
		if (conn_num > MAX_CONN_NUM)
			return nullptr;
		if (m_ConnPool == nullptr)
		{
			m_ConnPool = new MYSQLConnPool(host, user, psw, db, conn_num);
			if (m_ConnPool)
				return m_ConnPool;
			return nullptr;
		}
		return nullptr;
	}
	/* 成功返回该调用线程所使用连接的索引,出错返回mysql_query对应的返回值 */
	int Query(const char *sql);
	/* 根据Query返回值查询指定索引连接的返回结果 */
	MYSQL_RES *StoreResult(int idx);
	/* 释放结果后才能释放连接（使连接处于空闲可被调用） */
	void FreeResult(MYSQL_RES *res, int idx);

	~MYSQLConnPool()
	{
		for (int i = 0; i < m_ConnNum; ++i)
		{
			mysql_close(m_ConnVec[i].conn);
		}
	}

private:
	MYSQLConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num);

private:
	/* 连接池最大连接数 */
	static const int MAX_CONN_NUM = 16;
	/* 连接池的连接数 */
	int m_ConnNum;
	pthread_mutex_t m_ListMutex;
	sem_t m_ListSem;
	/* 连接列表 */
	std::vector<SMYSQL> m_ConnVec;

	static MYSQLConnPool *m_ConnPool;
};

MYSQLConnPool* MYSQLConnPool::m_ConnPool = nullptr;

MYSQLConnPool::MYSQLConnPool(const char *host, const char *user, const char *psw, const char *db, int conn_num)
{
	/* 根据连接数量初始化信号量 */
	int ret = sem_init(&m_ListSem, 0, conn_num);
	m_ListMutex = PTHREAD_MUTEX_INITIALIZER;
	m_ConnNum = conn_num;

	for (int i = 0; i < conn_num; ++i)
	{
		MYSQL *conn = mysql_init(nullptr);
		if (!conn)
			continue;
		conn = mysql_real_connect(conn, host, user, psw, db, 0, nullptr, 0);
		if (!conn)
			continue;
		m_ConnVec.push_back({false, conn});
	}
}

int MYSQLConnPool::Query(const char *sql)
{
	sem_wait(&m_ListSem);
	/* 有空闲连接，设置flag，相当于上锁 */
	int idx = 0;
	pthread_mutex_lock(&m_ListMutex);
	for (idx = 0; idx < m_ConnNum; ++idx)
	{
		if (!m_ConnVec[idx].flag)
		{
			m_ConnVec[idx].flag = true;
			break;
		}
	}
	pthread_mutex_unlock(&m_ListMutex);
	/* 执行查询 */
	int ret = mysql_query(m_ConnVec[idx].conn, sql);
	return ret == 0?idx:ret;
}

MYSQL_RES *MYSQLConnPool::StoreResult(int idx)
{
	return mysql_store_result(m_ConnVec[idx].conn);
}

void MYSQLConnPool::FreeResult(MYSQL_RES *res, int idx)
{
	mysql_free_result(res);
	m_ConnVec[idx].flag = false;
	sem_post(&m_ListSem);
}

#endif