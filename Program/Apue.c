/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：功能函数实现
*/

#include "Apue.h"

int sig_pipefd[2];

/* 设置套接字非阻塞 */
int gSetNonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

/* 往epoll描述符添加套接字 */
void gAddfd(int epollfd, int fd, bool oneshoot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET;
	/* 同一时刻只允许一个线程处理该描述符 */
	if (oneshoot)
	{
		event.events = event.events | EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	gSetNonblocking(fd);
}

/* 信号处理函数：对于父进程、子进程和所有其它进程，来自外界的信号都通过unix套接字传送到进程 */
void gSigHandler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(sig_pipefd[1], (char *)&msg, sizeof(int), 0);
	errno = save_errno;
}


void gAddSig(int sig, void (*handler)(int), bool restart)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	if (restart)
		sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1); 
}

/* 移除epoll内的一个描述符 */
void gRemovefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void gResetOneshot(int epollfd, int sockfd)
{
	epoll_event event;
	event.data.fd = sockfd;
	event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &event);
}
/*
*	默认转义字符为'\\'，分隔符为了'_'的文本串分离函数	
*/
std::vector<std::string> gSplitMsgPack(const char *text, char conver, char splitc)
{
	std::vector<std::string> pack;
	int tlen = strlen(text);
	char *str = (char *)malloc(sizeof(char)*tlen);
	int strindex = 0;
	int index = 0;
	bool isConverl = true;    // 是否'\\'
	bool isProtectConverl = true;		// 是否‘_’
	memset(str, 0, sizeof(char)*tlen);
	while (text[index] != '\0')
	{
		
		str[strindex] = text[index];
		if (strindex > 0)
		{
			if (str[strindex - 1] == conver && str[strindex] == conver)
			{
				if (isConverl)
				{
					str[strindex - 1] = str[strindex];
					strindex--;
				}
				isConverl = !isConverl;     /* 连着几个'\\'字符的情况 */
				isProtectConverl = true; 	/* 可能后面是连续几个'\\'，要保护好现场信息（通过保护isConverl值）*/
			}
			else if (str[strindex - 1] == conver && str[strindex] == splitc)
			{
				if (!isConverl) /* 如果'_'前面的'\\'字符是已经被转义了,那么这个'_'代表真正的分隔符 */
				{
		
					str[strindex] = '\0';
					pack.push_back(std::string(str));
					memset(str, 0, sizeof(char)*tlen);
					strindex = -1;
	
				}
				else	/* 这个'\\'的确是为了转义'_' */
				{
					str[strindex - 1] = str[strindex];
					strindex--;	
				}
				isProtectConverl = false;
			}
			else if (str[strindex - 1] != conver && str[strindex] == splitc)
			{
				str[strindex] = '\0';
				pack.push_back(std::string(str));
				memset(str, 0, sizeof(char)*tlen);
				strindex = -1;

				isProtectConverl = false;
			}
		}
		if (!isProtectConverl) /* 因为'\\'判别又重新开始 */
			isConverl = true;

		strindex++;
		index++;
	}
	str[strindex] = text[index];
	pack.push_back(std::string(str));
	free(str);
	return pack;
}