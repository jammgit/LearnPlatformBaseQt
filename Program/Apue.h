/*
*	作者：江伟霖
*	时间：2016/04/15
*	邮箱：269337654@qq.com or adalinors@gmail.com
*	描述：功能函数声明头文件
*/
#ifndef APUE_H
#define APUE_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vector>
#include <string>

extern int sig_pipefd[2];
int gSetNonblocking(int fd);
void gAddfd(int epollfd, int fd, bool oneshoot = false);
void gSigHandler(int sig);
void gAddSig(int sig, void (*handler)(int), bool restart = true);
void gRemovefd(int epollfd, int fd);
void gResetOneshot(int epollfd, int sockfd);
std::vector<std::string> gSplitMsgPack(const char *text, char conver = '\\', char splitc = '_');

#endif