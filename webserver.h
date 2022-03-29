/*************************************************************************
	> File Name: webserver.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月20日 星期六 17时15分14秒
 ************************************************************************/

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<sys/socket.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<cassert>
#include<stdlib.h>
#include<sys/epoll.h>
#include"./threadpool.h"
#include"./http/http_conn.h"
#include"CGImysql/sql_connection_pool.h"
#include"./timer/lst_timer.h"

const int MAX_EVENT_NUM = 100000;

class WebServer 
{
public:
	WebServer();
	~WebServer();

public:
	void init(int port, std::string userName, std::string passwd, std::string dbName, int log_asyn_write, int opt_linger, int trigmode, int sql_name, int thread_num, int close_log, int actor_model);
	void thread_pool();
	void sql_pool();
	void log_write();
	void trig_mode();
	void create_listensocket_eventListen();
	void eventLoop();
	void init_http_conn(int connfd, struct sockaddr_in client_address);
	void adjust_Timer(Timer *timer);
	void del_Timer(Timer *timer, int connfd);
	void dealclientconnection();
	bool dealwithsignal(bool &timerout, bool &stop_server);
	void dealwithread(int sockfd);
	void dealwithwrite(int sockfd);
private:
	int m_port;
	char *m_root;//指向当前文件夹下的root目录
	int m_log_asyn_write;//是否异步写入日志
	int m_close_log;
	int m_actor_model;//1:reactor 0:proactor

	int m_pipefd[2];//统一事件源，当定时信号被触发，信号处理函数向管道写数据，主流程epoll_wait监控
	int m_epollfd;
	http_conn *m_users_conn;//存储客户端连接信息

	//数据库相关
	connection_pool *m_connPool;
	std::string m_userName;
	std::string m_passwd;
	std::string m_dbName;
	int m_sql_conn_num;

	threadpool<http_conn> *m_threadpool;
	int m_thread_num;

	epoll_event m_events[MAX_EVENT_NUM];

	int m_listenfd;
	int m_opt_linger;//连接关闭模式，取值:0,1
	int m_TRIGMode;//listenfd和connfd组合出发模式,取值:0,1,2,3
	int m_listenTrigmode;//listenfd触发模式，取值:0,1
	int m_connTrigmode;//connfd触发模式，取值:0,1

	client_data *m_users_timer;//与每个m_users_conn相对应的定时任务数据
	TimerList *m_timerList;
};
#endif
