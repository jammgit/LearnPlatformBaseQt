#ifndef FILESERVER_H
#define FILESERVER_H

#include "BaseServer.h"
#include "MsgStruct.h"
#include "MsgType.h"
#include <fcntl.h>

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
int FileServer::DownloadFile(char *reqtext, int textlen)
{
	printf("DownloadFile\n");
	int fd = open(reqtext, O_RDONLY);
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