#include "ProcessPool.h"
#include "apue.h"
#include "CgiServer.h"



int main(int argc, char *argv[])
{
	int listenfd;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		write(STDERR_FILENO, "socket error\n", 13);
		exit(-1);
	}

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(8888);
	serv.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(listenfd, (struct sockaddr *)&serv, sizeof(serv));

	listen(listenfd, 32767);

	ProcessPool<CgiServer> *pCgi = ProcessPool<CgiServer>::Create(listenfd);

	if (pCgi)
	{
		pCgi->Run();
		delete pCgi;
	}
	close(listenfd);

	return 0;
}