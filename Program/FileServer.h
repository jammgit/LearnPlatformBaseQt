#ifndef FILESERVER_H
#define FILESERVER_H

#include "BaseServer.h"
#include "MsgStruct.h"
#include "MsgType.h"
#include "MYSQLConnPool.h"
#include <mysql.h>
#include <fcntl.h>
#include <stdlib.h>

/*	文件传输类
 *	
 *	初次计划的继承体系：
 *	BaseServer---|
 *				 |---FileServer---|---DownFileServer
 *				 |				  |---UpFileServer
 *								  |---ReloadFileServer
 *								  |
 *	鉴于这样会发生类爆发，故取消三个子类
 */
/*
*	对于文件存档，第一种是不同类型文档放不同数据库；
*/

/*
*	①任何出错均不主动关闭连接，返回 >0
*	②有可能通过过程文件服务类接受到FIN，则返回-1（控制线程管理套接字）
*/
class FileServer : public BaseServer
{
public:
	FileServer(int connfd, short msgtype)
		:BaseServer(connfd), m_MsgType(msgtype)
	{
		m_ConnPool = MYSQLConnPool::GetConnPoolPointer();
		if (!m_ConnPool)
		{
			m_ConnPool = MYSQLConnPool::CreateConnPool("localhost",
													 "root",
													 "root",
													 "LearnPlatformBaseQT",
													 4);	// 建立连接数量
		}
	};
	int Respond(char *reqtext, int textlen) final;
	/* 可能以后会添加子类增添传输方式 */
	virtual int UploadFile(char *reqtext, int textlen);
	virtual int DownloadFile(char *reqtext, int textlen);
	virtual int ReloadFile(char *reqtext, int textlen);
	virtual int DeleteFile(char *reqtext, int textlen);
private:
	/* 为避免代码重复率，回复时统一使用此函数,参数为回复的类型、文本 */
	int RespondClient(short type, const char *str)
	{
		msgPack msg;
		msg.type = htons(type);
		strcat(msg.text, str);
		msg.text[strlen(msg.text)] = '\0';
		int ret = send(m_connfd, reinterpret_cast<char *>(&msg), sizeof(msg.type) + strlen(msg.text), 0);
		/* 可能在 【客户端请求到达->服务类开始服务】 这段间隙，客户关闭的套接字 */
		while (ret < 0)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{/* 存储区满 */
				ret = send(m_connfd, reinterpret_cast<char *>(&msg), sizeof(msg.type) + strlen(msg.text), 0);
			}
			else if (errno == EPIPE)
			{/* 客户端关闭，写导致爆管 */
				return -1;
			}
		}
		return ret;
	}
private:
	short m_MsgType;
	MYSQLConnPool *m_ConnPool;
private:	/* 一些文件服务对请求文本的格式要求 */
	const int UPLOAD_SEGS = 3;
	const int DOWNLOAD_SEGS = 2;
	const int DELETE_SEGS = 2;
	const int RELOAD_SEGS = 3;
	/* 如：下载文件时，请求信息包的文本中应该包含两个信息段，用指定分隔符分隔 */
	const int RELOAD_RETURN_NUM = 20; //默认每次返回用户多少条记录

};

int FileServer::Respond(char *reqtext, int textlen)
{
	int ret;
	switch(m_MsgType)
	{
		case MSG_RELOAD_FILE:
			ret = ReloadFile(reqtext, textlen);
			break;
		case MSG_DOWNLOAD_FILE:
			ret = DownloadFile(reqtext, textlen);
			break;
		case MSG_UPLOAD_FILE:
			ret = UploadFile(reqtext, textlen);
			break;
		case MSG_DELETE_FILE:
			ret = DeleteFile(reqtext, textlen);
			break;
		default:
			break;
	}
	return ret;
}

/*
*	上传文件应该先完全接收完后再将主要信息插入数据库
*	动作：
*	回复(MSG_OK)客户端开始传输(上传过程type为MSG_UPLOAD_FILE，完成时type为MSG_OK)，然后接受数据
*	account + '_' + filename + '_' + SjNumber
*/
int FileServer::UploadFile(char *reqtext, int textlen)
{
	printf("Start upload function\n");
	std::vector<std::string> pack = gSplitMsgPack(reqtext);
	if (pack.size() != UPLOAD_SEGS)
	{/* 发送的数据格式有错 */
		return RespondClient(MSG_ERROR, "请求格式错误,正确格式为:用户名_文件名_学科号");
	}

	printf("%s_%s_%s\n", pack[0].c_str(), pack[1].c_str(), pack[2].c_str());
	/* 数据库操作 */
	MYSQL *conn = m_ConnPool->GetConnection();
	char buffer[256];
	sprintf(buffer, "select * from T_SourceInfo where account='%s' and file_name='%s';",
		pack[0].c_str(),
		pack[1].c_str());
	int ret = mysql_query(conn, buffer);
	if (ret < 0)
	{
		printf("mysql query error\n");
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "服务器数据库查询出错");	
	}
	MYSQL_RES *res = mysql_store_result(conn);
	MYSQL_ROW row = mysql_fetch_row(res); // 获取一行
	if (row)
	{/* 如果已存在同名称、同上传者，则不允许上传 */
		printf("File have exist\n");
		mysql_free_result(res);
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "此用户已存在同名文件");
	}
	mysql_free_result(res);
	/* 归还连接 */
	m_ConnPool->RecoverConnection(conn);

	/* 回复可以上传该文件 */
	ret = RespondClient(MSG_OK, "");
	if (ret < 0)
	{/* 爆管 */
		return ret;
	}

	/* 开始接受文件[考虑：本地处理快过网络传输] */
	printf("Start receive:\n");
	char fname[128];
	sprintf(fname, "./ServerSourceArea/%s_%s", pack[0].c_str(), pack[1].c_str());
	msgPack msg;
	int fd = open(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	int nR;
	while ((nR = recv(m_connfd, reinterpret_cast<char *>(&msg), sizeof(msg), 0)) != 0)
	{/* nR<0 是因为缓冲区无数据可读，目前没想到更好的办法 */
		if (nR > 0)
		{/* 接受到数据 */
			if (ntohs(msg.type) == MSG_UPLOAD_FILE)
			{
				printf("Receive oneline\n");
				write(fd, msg.text, strlen(msg.text));
			}
			else if (ntohs(msg.type) == MSG_OK)
			{/* 上传完成 */
				printf("Receive finish\n");
				close(fd);
				time_t t;
				time(&t);
				sprintf(buffer, "insert into T_SourceInfo values('%s', %ld, '%s', 0, '%s', %d);",
					pack[1].c_str(),
					t,
					pack[0].c_str(),
					fname,
					atoi(pack[2].c_str()));
				conn = m_ConnPool->GetConnection();
				ret = mysql_query(conn, buffer);
				if (ret < 0)
				{
					printf("insert error\n");
					m_ConnPool->RecoverConnection(conn);
					return RespondClient(MSG_ERROR, "数据库更新出现错误，请重新操作,正确请求格式:用户名_文件名_学科号");
				}
				m_ConnPool->RecoverConnection(conn);
				break;

			}
		}
	}
	if (nR == 0)
	{/* 客户端断开连接 */
		close(fd);
		return -1;
	}
	
	return 1;
}

/*
*	下载请求时，请求包text文本格式：
*	account + '_' + file_name
*	动作：
*	客户端发送请求包后，接收文件数据；关不关闭此套接字由客户端编程决定
*/
int FileServer::DownloadFile(char *reqtext, int textlen)
{
	/* 数据库查找是否存在此文件，若存在找出其完整路径名 */
	std::vector<std::string> pack = gSplitMsgPack(reqtext);
	if (pack.size() != DOWNLOAD_SEGS)
	{	/* 请求包信息错误 */
		return RespondClient(MSG_ERROR, "请求格式错误，正确格式:用户名_文件名");
	}

	printf("%s_%s\n",pack[0].c_str(), pack[1].c_str());
	char buffer[256];
	sprintf(buffer, "select path_name from T_SourceInfo where file_name='%s' and account='%s';",
			pack[1].c_str(), pack[0].c_str());

	/* 搜索是否存在请求的资源 */
	MYSQL *conn = m_ConnPool->GetConnection();
	int ret = mysql_query(conn, buffer);
	if (ret < 0)
	{
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "数据库操作出错，请重试！正确请求格式:用户名_文件名");
	}
	MYSQL_RES *res = mysql_store_result(conn);
	MYSQL_ROW row = mysql_fetch_row(res);/* 检索行 */
	unsigned int fields = mysql_num_fields(res);
	if (!row)
	{/* 不存在 */
		m_ConnPool->RecoverConnection(conn);
		mysql_free_result(res);
		return RespondClient(MSG_ERROR, "请求资源不存在，请重试！正确请求格式:用户名_文件名");
	}
	m_ConnPool->RecoverConnection(conn);
	mysql_free_result(res);

	if (row)
	{
		printf("\nStart\n");
		for (int i = 0; i < fields; ++i)
			printf("%s\n", row[i]?row[i]:"NULL");
		printf("End\n");
	}


	printf("DownloadFile\n");
	/* 开始传输文件 */
	int fd = open(row[0], O_RDONLY);
	if (fd < 0)
	{
		printf("open file failed\n");
		return RespondClient(MSG_ERROR, "服务器本地打开文件错误，请重试！正确请求格式:用户名_文件名");
	}
	int nR, nW;
	char text[TEXTSIZE];
	//msg.type = htons(MSG_DOWNLOAD_FILE);
	while ((nR = read(fd, text, sizeof(text) - 1)) >= 0)
	{
		printf("Read file\n");
		text[nR] = '\0';
		if (nR == 0) /* 到文件尾 */
		{
			ret = RespondClient(MSG_OK, text);
			if (ret < 0)/* 爆管 */
				break;
		}
		else
		{
			ret = RespondClient(MSG_DOWNLOAD_FILE, text);
			if (ret < 0)/* 爆管 */
				break;
		}
		printf("Write [%d]byte to sock\n", ret);
	}
	if (ret < 0)
	{
		close(fd);
		return ret;
	}
	if (nR < 0)
	{/* 读文件错误 */
		close(fd);
		return RespondClient(MSG_ERROR, "服务器读文件发生错误，请重试!请求格式:用户名_文件名");
	}

	sprintf(buffer, "update T_SourceInfo set download_count=download_count+1 where file_name='%s' and account='%s';",
		pack[1].c_str(),
		pack[0].c_str());
	conn = m_ConnPool->GetConnection();
	ret = mysql_query(conn, buffer);
	if (ret < 0)
	{/* 如果真出错怎么办 ...*/
		m_ConnPool->RecoverConnection(conn);
		return 1;
	}
	m_ConnPool->RecoverConnection(conn);

	return 1;
}
/*
*	将数据库文件表的信息全息发送给客户端
*	请求格式：
*	（全部资源=0|某一学科>0）整形 + '_' + (时间0|热度1)整形 + ‘_' + （请求起始索引>=0）整形
*	回复格式：
*	上传者 + '_' + 文件名 + '_' + 上传时间 + '_' + 热度[下载次数] (已排好序，按时间/热度)，记录之间使用'+'间隔
*/
int FileServer::ReloadFile(char *reqtext, int textlen)
{
	std::vector<std::string> pack = gSplitMsgPack(reqtext);
	if (pack.size() != RELOAD_SEGS)
	{
		return RespondClient(MSG_ERROR, "请求格式错误，正确请求格式:全部0/学科>0_时间0/热度1_索引数)");
	}

	char cate[16];
	if (strncmp(pack[1].c_str(), "0", 1) == 0)//按时间
		strcpy(cate, "upload_time");
	else//按热度
		strcpy(cate, "download_count");

	char buffer[256];
	if (strncmp(pack[0].c_str(), "0", 1) == 0)
	{/* 全部资源 */
		sprintf(buffer, "select account,file_name,upload_time,download_count from T_SourceInfo order by %s desc;",
		 		cate);
	}
	else/* 针对某个学科 */
	{
		sprintf(buffer, "select account,file_name,upload_time,download_count from T_SourceInfo where %d=SjNumber order by %s desc;",
				atoi(pack[0].c_str()), cate);
	}

	/* 开始查询 */
	MYSQL *conn = m_ConnPool->GetConnection();
	int ret = mysql_query(conn, buffer);
	if (ret < 0)
	{
		printf("mysql query error\n");
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "服务器出错，请重试");
	}
	/* 该立刻释放conn吗，就算还要用res，资源释放顺序？ */
	MYSQL_RES *res = mysql_store_result(conn);
	MYSQL_ROW row;
	/* 去掉指定索引前的记录 */
	int index = atoi(pack[2].c_str());
	for (int i = 0; i < index - 1; ++i)
	{
		if (row)
			row = mysql_fetch_row(res);
		else
		{/* 没有再多记录了（用户都加载过了） */
			// 处理 ...
			mysql_free_result(res);
			m_ConnPool->RecoverConnection(conn);
			return RespondClient(MSG_OK, "");
		}
	}

	/* 在剩余的记录中找到指定条数记录并回复给用户 */
	char text[TEXTSIZE];
	memset(text, 0, TEXTSIZE);
	unsigned int fields = mysql_num_fields(res);
	int count = 0; //计数器，返回一条记录最长是170字节，所以一条信息至少可以包含3条记录
	int i;
	for (i = 0; i < RELOAD_RETURN_NUM; ++i)
	{
		row = mysql_fetch_row(res);
		if (row)
		{
			for (int j = 0; j < fields; ++j)
			{
				strcat(text, row[j]);
				strcat(text, "_");
			}

			count++;
			if (count == 3)
			{/* 回复一条信息 */
				text[strlen(text)-1] = '\0';
				ret = RespondClient(MSG_RELOAD_FILE, text);
				if (ret < 0)
				{
					mysql_free_result(res);
					m_ConnPool->RecoverConnection(conn);
					return ret;
				}
				memset(text, 0, TEXTSIZE);
				count=0;	/*新信息的计数值*/
			}
			else
				text[strlen(text)-1] = '+';
		}
		else
		{/* 就算文本为空，也是返回MSG_OK */
			mysql_free_result(res);
			res = nullptr;
			m_ConnPool->RecoverConnection(conn);
			int l = strlen(text);
			if (l != 0)
				text[l-1]='\0';
			ret = RespondClient(MSG_OK, text);
			if (ret < 0)
			{
				mysql_free_result(res);
				m_ConnPool->RecoverConnection(conn);
				return ret;
			}
			break;
		}
	}
	if (res)
		mysql_free_result(res);
	m_ConnPool->RecoverConnection(conn);
	/* 记录多于k条，那么最后一个MSG_OK包还没发送*/
	if (i == RELOAD_RETURN_NUM)
	{
		int l = strlen(text);
		if (l != 0)
			text[l-1] = '\0';
		ret = RespondClient(MSG_OK, text);
		if (ret < 0)
			return ret;
	}
	return 1;
}

/*
*	应先立刻删除数据库数据，再删除相应文件
*	格式：account + '_' + file_name
*	动作：
*	查找并删除,回复客户端成功或者失败
*/
int FileServer::DeleteFile(char *reqtext, int textlen)
{
	std::vector<std::string> pack = gSplitMsgPack(reqtext);
	if (pack.size() != DELETE_SEGS)
	{
		return RespondClient(MSG_ERROR, "请求格式错误，正确请求格式:用户名_文件名");
	}

	char buffer[256];
	sprintf(buffer, "select path_name from T_SourceInfo where account='%s' and file_name = '%s';",
			pack[0].c_str(), pack[1].c_str());

	MYSQL *conn = m_ConnPool->GetConnection();
	int ret = mysql_query(conn, buffer);
	if (ret < 0)
	{
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "请求失败，请继续尝试");
	}

	MYSQL_RES *res = mysql_store_result(conn);
	MYSQL_ROW row = mysql_fetch_row(res);
	if (!row)
	{/* 并没有此文件 */
		m_ConnPool->RecoverConnection(conn);
		mysql_free_result(res);
		return RespondClient(MSG_ERROR, "请重试，请求格式:用户名_文件名");
	}

	mysql_free_result(res);
	/* 删除文件/记录 */
	sprintf(buffer, "delete from T_SourceInfo where account='%s' and file_name='%s';", 
		pack[0].c_str(),
		pack[1].c_str());
	ret = mysql_query(conn, buffer);
	if (ret < 0)
	{
		m_ConnPool->RecoverConnection(conn);
		return RespondClient(MSG_ERROR, "请重试，请求格式:用户名_文件名");	
	}
	m_ConnPool->RecoverConnection(conn);

	/* 先回复MSG_OK,剩余删除真实文件是服务器内部自己决定的事了 */
	ret = RespondClient(MSG_OK, "");
	/* 这里有个问题：如果删除时正有人在下载该文件，如何处理？ */
	remove(row[0]);
	
	return ret<0?ret:1;
}


#endif