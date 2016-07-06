
/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：服务器主程序
*/

#include "ProcessPool.h"
#include "XServer.h"
#include "MYSQLConnPool.h"



int main(int argc, char *argv[])
{
	int listenfd;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		write(STDERR_FILENO, "socket error\n", 13);
		exit(-1);
	}

	int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(8888);
	serv.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(listenfd, (struct sockaddr *)&serv, sizeof(serv));

	listen(listenfd, 32767);

	ProcessPool<XServer> *pCgi = ProcessPool<XServer>::Create(listenfd);

	if (pCgi)
	{
		pCgi->Run();
		delete pCgi;
	}
	close(listenfd);

	return 0;
}