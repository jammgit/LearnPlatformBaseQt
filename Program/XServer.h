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

/*
*	对于网络通信出错，要关闭套接字使：Process 返回-1
*	
*/

class XServer
{


public:
	XServer(int connfd):m_connfd(connfd) {};
	/* 进程将剩余任务交给子进程，然后自己立刻回到epoll_wait状态 */
	int Process()
	{
		int ret = 0;
		msgPack msg;
		if ((ret = recv(m_connfd, (char *)&msg, sizeof(msgPack), 0)) >= 0)
		{
			if (ret == 0)
				return -1;
			/* 开始处理事件 */
			switch(ntohs(msg.type))
			{	/* 回射服务 */
				case MSG_ECHO:
				{	
					printf("process echo\n");
					strcat(msg.text, "(You send to server)\n");	
					ret = RespondClient(MSG_ECHO, msg.text);
					break;
				}/* 文件服务 */
				case MSG_DOWNLOAD_FILE:
				case MSG_UPLOAD_FILE:
				case MSG_RELOAD_FILE:
				case MSG_DELETE_FILE:
					printf("process file:%s\n", msg.text);
					ret = subProcess(new FileServer(m_connfd, ntohs(msg.type)), msg.text, strlen(msg.text));
					break;
				default:
					break;
			}
			if (ret < 0)
				return -1;

		}
		return 1;
	}
	int GetConnfd()
	{
		return m_connfd;
	}
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
	/* 抽象工厂！分用函数，text为用户请求的消息文本 */
	int subProcess(BaseServer *base, char *text, int textlen);
private:
	int m_connfd;
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