/*************************************************************************
	> File Name: lst_timer.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月14日 星期日 16时01分45秒
 ************************************************************************/

#ifndef TIMER_H
#define TIMER_H 

#include<unistd.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<ctime>

#define BUF_SIZE 64

class Timer;

struct client_data 
{
	struct sockaddr_in address;
	int confd;
	char buf[BUF_SIZE];
	Timer *timer;
};

class Timer 
{
	friend class TimerList;
public:
	typedef void(*cbptr)(int epfd, client_data *);
public:
	Timer(time_t expiretime, cbptr cb, client_data *usrdata,
			Timer *m_pre = nullptr, Timer *m_next = nullptr):
		m_expiretime(expiretime), m_userdata(usrdata), call_back(cb){}
	void callback(int epfd){call_back(epfd, m_userdata);}
	~Timer(){}
	
public:
	void setExpireTime(time_t t){ m_expiretime = t; }
private:
	time_t m_expiretime;
	client_data *m_userdata;
	cbptr call_back;
	Timer *m_pre;
	Timer *m_next;
};

class TimerList
{
public:
	TimerList(int epfd):m_epollfd(epfd){}
	~TimerList(){}
public:
	void addTimer(Timer *timer);
	void delTimer(Timer *timer);
	void adjust(Timer *timer);
	void tick();
private:
	Timer *head = nullptr;
	Timer *tail = nullptr;
	int m_epollfd;
};
#endif
