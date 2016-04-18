
#ifndef BASESERVER_H
#define BASESERVER_H

/*
*	抽象工厂，所有服务类型的抽象基类
*/
class BaseServer
{
public:
	BaseServer(int connfd) : m_connfd(connfd) {};
	virtual int Respond(char *reqtext, int textlen) = 0;
protected:
	int m_connfd;
};


#endif