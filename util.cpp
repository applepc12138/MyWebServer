/*************************************************************************
	> File Name: util.cpp
	> Author: ggboypc12138 
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月08日 星期一 15时18分15秒 
 ************************************************************************/

#include"util.h"
#include"timer/lst_timer.h"
#include<csignal>
#include<sys/socket.h>
#include<sys/types.h>
#include<errno.h>

int *utils::u_pipefd = nullptr;

int utils::setunblocking(int fd)
{
	int old_opt = fcntl(fd, F_GETFL);
	int new_opt = O_NONBLOCK | old_opt;
	fcntl(fd, F_SETFL, new_opt);
	return old_opt;
}

void utils::addfd(int epfd, int fd, uint32_t event = EPOLLIN, 
		int TRIGMode = 1, bool one_shot = true)
{
	epoll_event e;
	e.data.fd = fd;
	if(TRIGMode == 1)//边沿触发
		event |= EPOLLET | EPOLLRDHUP;//
	else 
		event |=  EPOLLRDHUP;
	if(one_shot == true){
		event |= EPOLLONESHOT;
	}
	e.events = event;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
	setunblocking(fd);
}

bool utils::removefd(int epfd, int fd)
{
	return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
	close(fd);
	
}

void utils::modfd(int epfd, int fd, uint32_t event, int TRIGMode)
{
	if(TRIGMode == 1)//边沿触发
		event |= EPOLLET | EPOLLRDHUP | EPOLLONESHOT;//
	else 
		event |=  EPOLLRDHUP | EPOLLONESHOT;
	epoll_event e;
	e.data.fd = fd;
	e.events = event;
	epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e);
}

void utils::addsigToHandle(int sig, void(handler)(int), bool restart)
{
	struct sigaction sigact;
	bzero(&sigact, sizeof(sigact));
	if(restart)//重启被信号打断的系统调用
		sigact.sa_flags |= SA_RESTART;
	sigact.sa_handler = handler;
	sigfillset(&sigact.sa_mask);
	sigaction(sig, &sigact, NULL);
}

//信号处理函数
void utils::sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;//????
	send(u_pipefd[1], (char *)&msg, 1, 0);
	errno = save_errno;
}
