/*************************************************************************
	> File Name: util.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月08日 星期一 20时24分28秒
 ************************************************************************/

#ifndef UTIL_H
#define UTIL_H 

#include<unistd.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<signal.h>
#include<strings.h>

class utils{
public:
	static int setunblocking(int fd);
	static void addfd(int epfd, int fd, uint32_t event, int TRIGMode, bool one_shot);
	static bool removefd(int epfd, int fd);
	static void modfd(int epfd, int fd, uint32_t event, int TRIGMode);
	
	static void addsigToHandle(int sig, void(handler)(int), bool restart = true);
	static void sig_handler(int sig);
public:
	static int *u_pipefd;

};
#endif
