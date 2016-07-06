/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：服务器测试程序
*/

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/un.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <vector>
#include <string>

#include "../MsgStruct.h"
#include "../MsgType.h"

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
//#define path "tempfile.socket"

void sig(int sig)
{
	char buf[512];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "write error, ertrno(%d) -> %s\n", errno, strerror(errno));
	write(STDERR_FILENO, buf, strlen(buf));
	sleep(3);
	exit(-1);
		
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage:./exec [port]\n");
		exit(-1);
	}

	char buf[512];
	memset(buf, 0, sizeof(buf));
	short type;
	int which;
	printf("Please input type:1[echo] 2[download file] 3[delete file] 4[upload file] 5[Reload file]");
	scanf("%d", &which);
	switch(which)
	{
		case 1:
			type = MSG_ECHO;
			strcpy(buf, "Please input string:");
			break;
		case 2:
			type = MSG_DOWNLOAD_FILE;
			strcpy(buf, "Please input filename:");
			break;
		case 3:
			type = MSG_DELETE_FILE;
			strcpy(buf, "Please input filename:");
			break;
		case 4:
			type = MSG_UPLOAD_FILE;
			strcpy(buf, "Please input filename:");
			break;
		case 5:
			type = MSG_RELOAD_FILE;
			strcpy(buf, "Please input 'format'(（全部资源=0|某一学科>0）整形 + '_' + (时间0|热度1)整形 + ‘_' + （请求起始索引>=0）整形):");
			break;
		default:
			break;
	}
	fflush(stdout);


	signal(SIGPIPE, sig);
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = inet_addr("192.168.1.115");

	connect(sock, (struct sockaddr *)&addr, sizeof(addr));

	int old = fcntl(sock, F_GETFL);
	int newoption = old | O_NONBLOCK;
	fcntl(sock, F_SETFL, newoption);



	int epollfd = epoll_create(5);
	epoll_event event[128];
	epoll_event e1,e2;
	e1.data.fd = sock;
	e1.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &e1); 
	e2.data.fd = STDIN_FILENO;
	e2.events = EPOLLIN;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &e2); 


	int nR;
	int ret;
	msgPack msg;
	int fd;
	bool isp = true;

	while (1)
	{
		if (isp)
		{
			printf("%s", buf);
			fflush(stdout);
		}

		int number = epoll_wait(epollfd, event, 128, -1);
		if (number <= 0)
		{
			printf("epoll_wait error\n");
			break;
		}

		for (int i = 0; i < number; ++i)
		{
			if (event[i].data.fd == sock && (event[i].events & EPOLLIN))
			{
				memset(&msg, 0, sizeof(msg));
				nR = read(sock, (char *)&msg, sizeof(msg));
				if (nR == 0)
				{
					close(sock);
					break;
				}

				if (ntohs(msg.type) == MSG_DOWNLOAD_FILE)
				{
					fd = open("file", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
					write(fd, msg.text, strlen(msg.text));
					close(fd);
					close(sock);
					exit(0);
				}
				else if(ntohs(msg.type) == MSG_OK)
				{
					if (type == MSG_RELOAD_FILE)
					{
						std::vector<std::string> pack1 = gSplitMsgPack(msg.text, '\\', '+');
						size_t size = pack1.size();
						for (size_t i = 0; i < size; ++i)
						{
							std::vector<std::string> pack2 = gSplitMsgPack(pack1[i].c_str(), '\\', '_');
							printf("%-15s %-15s %-15s %-15s\n",
									pack2[0].c_str(),
									pack2[1].c_str(),
									pack2[2].c_str(),
									pack2[3].c_str());

						}
						exit(0);
					}
					else
						printf("Input string in stdin imitating the file(end with string 'EOF'):\n");


				}
				else if(ntohs(msg.type) == MSG_RELOAD_FILE)
				{
					std::vector<std::string> pack1 = gSplitMsgPack(msg.text, '\\', '+');
					size_t size = pack1.size();
					for (size_t i = 0; i < size; ++i)
					{
						std::vector<std::string> pack2 = gSplitMsgPack(pack1[i].c_str(), '\\', '_');
						printf("%-15s %-15s %-15s %-15s\n",
								pack2[0].c_str(),
								pack2[1].c_str(),
								pack2[2].c_str(),
								pack2[3].c_str());

					}

				}
				else
				{
					write(STDOUT_FILENO, msg.text, strlen(msg.text));
					isp = true;
				}
			}
			else if (event[i].data.fd == STDIN_FILENO && (event[i].events & EPOLLIN))
			{
				msg.type = htons(type);
				memset(msg.text, 0, sizeof(msg.text));
				nR = read(STDIN_FILENO, msg.text, TEXTSIZE);
				printf("You have input:%s\n",msg.text);
				if (strncmp("EOF", msg.text, 3) == 0)
				{
					printf("check EOF\n");
					msg.type = htons(MSG_OK);
				}
				// 去掉换行符
				msg.text[nR - 1] = '\0';
				if (ntohs(msg.type) == MSG_RELOAD_FILE)
				{
					printf("%-15s %-15s %-15s %-15s\n",
						"account","file_name","upload_time","download_count");
				}
									
				ret = write(sock, (char *)&msg, sizeof(msg.type) + strlen(msg.text));
				if (ret < 0)
				{
					close(sock);
					printf("write sock ret < 0\n");
					exit(-1);
				}
				if (ntohs(msg.type) == MSG_OK)
				{
					printf("upload success, you can check server file\n");
					exit(0);
				}
				isp = false;
			}
		}
	}

	close(sock);

	return 0;
}
