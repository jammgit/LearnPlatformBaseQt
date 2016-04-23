#ifndef FILESERVER_H
#define FILESERVER_H

#include "BaseServer.h"
#include "MsgStruct.h"
#include "MsgType.h"
#include "MYSQLConnPool.h"
#include <fcntl.h>
#include <mysql.h>

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


class FileServer : public BaseServer
{
public:
	FileServer(int connfd, short msgtype)
		:BaseServer(connfd), m_MsgType(msgtype)
	{};
	int Respond(char *reqtext, int textlen);
	/* 可能以后会添加子类增添传输方式 */
	virtual int UploadFile(char *reqtext, int textlen);
	virtual int DownloadFile(char *reqtext, int textlen);
	virtual int ReloadFile(char *reqtext, int textlen);
private:
	short m_MsgType;

private:	/* 一些文件服务对请求文本的格式要求 */
	const int UPLOAD_SEGS = 3;
	const int DOWNLOAD_SEGS = 2;	/* 如：下载文件时，请求信息包的文本中应该包含两个信息段，用指定分隔符分隔 */
	const int RELOAD_SEGS = 2;	

};

int FileServer::Respond(char *reqtext, int textlen)
{
	int ret;
	switch(ntohs(m_MsgType))
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
		default:
			break;
	}
	return ret;
}

int FileServer::UploadFile(char *reqtext, int textlen)
{
	return 0;
}

/*
*	下载请求时，请求包text文本格式：
*	account + '_' + file_name
*/
int FileServer::DownloadFile(char *reqtext, int textlen)
{

	std::vector<std::string> pack = gSplitMsgPack(reqtext);
	printf("%s_%s\n",pack[0].c_str(), pack[1].c_str());
	if (pack.size() != DOWNLOAD_SEGS)
	{	/* 请求包信息错误 */
		// ...
	}
	char buffer[256];
	sprintf(buffer, "select path_name from T_SourceArea where file_name='%s' and account='%s';",
			pack[1].c_str(), pack[0].c_str());
	int idx = connpool->Query(buffer);
	if (idx < 0)
	{
		printf("Mysql pool error\n");
		return m_connfd;
	}
	MYSQL_RES *res = connpool->StoreResult(idx);
	if (!res)
	{
		printf("Mysql result error\n");
		return m_connfd;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	unsigned int fields = mysql_num_fields(res);
	if (row)
	{
		for (int i = 0; i < fields; ++i)
			printf("%s\n", row[i]?row[i]:"NULL");
	}

	printf("DownloadFile\n");
	int fd = open(row[0], O_RDONLY);
	if (fd < 0)
		printf("open file failed\n");
	int nR;
	msgPack msg;
	msg.type = htons(MSG_DOWNLOAD_FILE);
	while ((nR = read(fd, msg.text, sizeof(msg.text) - 1)) > 0)
	{
		printf("Read file\n");
		if (nR < 0)
		{
			if (errno == EINTR)
				continue;	
			else
			{
				close(m_connfd);
				return -1;
			}
		}
		msg.text[nR] = '\0';

		nR = write(m_connfd, (char *)&msg, sizeof(msg.type) + strlen(msg.text));
		if (nR > 0)
			printf("Write file to sock\n");
	}

	close(fd);
	return m_connfd;
}

int FileServer::ReloadFile(char *reqtext, int textlen)
{
	return 0;
}

#endif