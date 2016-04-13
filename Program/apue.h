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

extern int sig_pipefd[2];
int gSetNonblocking(int fd);
void gAddfd(int epollfd, int fd);
void gSigHandler(int sig);
void gAddSig(int sig, void (*handler)(int), bool restart = true);
void gRemovefd(int epollfd, int fd);


#endif