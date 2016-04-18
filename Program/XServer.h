/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：简单回射服务
*/

#ifndef CGISERVER_H
#define CGISERVER_H

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

#include "Apue.h"
#include "FileServer.h"

#include "BaseServer.h"

#include "MsgStruct.h"
#include "MsgType.h"

class XServer
{
private:
	int m_connfd;

public:
	XServer(int connfd):m_connfd(connfd) {};
	/* 进程将剩余任务交给子进程，然后自己立刻回到epoll_wait状态 */
	int Process()
	{
		int nRead = 0;
		char sendbuf[TEXTSIZE + sizeof(short) + 32];
		msgPack msg;
		while (1)
		{
			if (nRead < 0)	/* 在下面处理事件循环回来后，套接字已经是关闭状态 */
				return -1;
			memset(&msg, 0, sizeof(msgPack));
			nRead = recv(m_connfd, (char *)&msg, sizeof(msgPack), 0);

			if (nRead == 0)
			{
				close(m_connfd);
				return -1;
			}
			else if (nRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			{
				return m_connfd;
			}
			/* 开始处理事件 */
			switch(ntohs(msg.type))
			{	/* 回射服务 */
				case MSG_ECHO:
				{	
					printf("process echo\n");
					memset(sendbuf, 0, sizeof(sendbuf));
					strcat(msg.text, "(You send to server)\n");
					send(m_connfd, (char *)&msg, strlen((char *)&msg), 0);					
					break;
				}/* 文件服务 */
				case MSG_DOWNLOAD_FILE:
				case MSG_UPLOAD_FILE:
				case MSG_RELOAD_FILE:
					printf("process file:%s\n", msg.text);
					nRead = subProcess(new FileServer(m_connfd, msg.type), msg.text, strlen(msg.text));
					break;
				default:
					break;
			}
			
			

		}

	}
private:
	/* 抽象工厂！分用函数，text为用户请求的消息文本 */
	int subProcess(BaseServer *base, char *text, int textlen);

};

int XServer::subProcess(BaseServer *base, char *text, int textlen)
{
	printf("subProcess\n");
	int ret = base->Respond(text, textlen);

	if (base)
	{
		delete base;
		base = nullptr;
	}

	return ret;
}


#endif