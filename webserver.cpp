/*************************************************************************
	> File Name: webserver.cpp
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月20日 星期六 17时14分45秒 ************************************************************************/

#include"webserver.h"
#include"util.h"

const int MAX_FD = 65536;
//const int MAX_EVENT_NUM = 100000;
const int TIMESLOT = 5;

WebServer::WebServer()
{
	m_users_conn = new http_conn[MAX_FD];
	
	//root文件夹路径
	char cur_path[200];
	getcwd(cur_path, 200);
	m_root = (char *)malloc(strlen(cur_path) + 5 + 1);
	strcpy(m_root, cur_path);
	strcat(m_root, "/root");

	//定时器
	m_users_timer = new client_data[MAX_FD];//????
}

WebServer::~WebServer()
{
	close(m_epollfd);
	close(m_listenfd);
	close(m_pipefd[1]);
	close(m_pipefd[0]);

	delete m_users_conn;
	delete m_users_timer;
	delete m_threadpool;
	delete m_root;
}

void WebServer::init(int port, std::string userName, std::string passwd, std::string dbName, int log_asyn_write, int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
	m_port = port;
    m_userName = userName;
    m_passwd = passwd;
    m_dbName = dbName;
    m_sql_conn_num = sql_num;
    m_thread_num = thread_num;
    m_log_asyn_write = log_asyn_write;
    m_opt_linger = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void WebServer::trig_mode()
{
   //LT + LT
    if (0 == m_TRIGMode)
    {
        m_listenTrigmode = 0;
        m_connTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_listenTrigmode = 0;
        m_connTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_listenTrigmode = 1;
        m_connTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_listenTrigmode = 1;
        m_connTrigmode = 1;
    }
}

void WebServer::log_write()
{
	if(m_close_log == 0){
		if(m_log_asyn_write == 1)
			Log::get_intance()->init("./ServerLog", m_close_log, 2048, 80000, 800);
		else
			Log::get_intance()->init("./ServerLog", m_close_log, 2048, 80000, 0);
	}
}

void WebServer::sql_pool()
{
	//初始化数据库连接池
	m_connPool = connection_pool::get_instance();
	m_connPool->init("localhost", m_userName, m_passwd, m_dbName, 3306, m_sql_conn_num, m_close_log);
	
	//读表到http连接类中m_users字段????
	m_users_conn->initmysql_result(m_connPool);//????
}

void WebServer::thread_pool()
{
	//初始化线程池
	m_threadpool = new threadpool<http_conn>(m_actor_model, m_connPool, m_thread_num);
}

void WebServer::create_listensocket_eventListen()
{
	m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(m_listenfd >= 0);
	//P159
	//优雅关闭		
	//默认情况下，close操作立即返回，若有数据残留在套接字发送缓冲中，系统将试着吧这些数据发送给对端连接
	//SO_LINGER选项可以改变这个默认设置
	/*struct linger{ 
		int l_onoff;
		int l_linger;
	 * */
	if(m_opt_linger == 1){		
		//若有残留数据在发送缓冲区中，进程被挂起直至所有数据发送完且被对方确认或延迟时间到
		//若套接字被设为非阻塞，则不等待
		struct linger tmp = {1, 1};
		setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp,	sizeof(tmp));
	}		

	int flag = 1;
	//SO_REUSEADDR针对在服务器主动关闭连接后处于time-wait状态时(linux系统time-wait持续时间为1min)，确保服务器重启成功
	setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	
	//绑定套接字
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(m_port);
	int ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret >= 0);
	ret = listen(m_listenfd, 5);
	assert(ret >= 0);
	

	//epoll_event events[MAX_EVENT_NUM];
	m_epollfd = epoll_create(5);
	assert(m_epollfd != -1);
	http_conn::m_epollfd = m_epollfd;
	utils::addfd(m_epollfd, m_listenfd, EPOLLIN, m_listenTrigmode, true);
	
	//初始化定时任务链表
	m_timerList = new TimerList(m_epollfd);

	//创建管道，统一事件源
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);

	utils::u_pipefd = m_pipefd;

	//信号处理函数向m_pipefd[1]，主流程从m_pipefd[0]读信息
	utils::setunblocking(m_pipefd[1]);
	utils::addfd(m_epollfd, m_pipefd[0], EPOLLIN, 0, false);

	//设置信号
	utils::addsigToHandle(SIGPIPE, SIG_IGN);
	utils::addsigToHandle(SIGALRM, utils::sig_handler, false);
	utils::addsigToHandle(SIGTERM, utils::sig_handler, false);
	alarm(TIMESLOT);
};

void cb_func(int epfd, client_data *user_data)
{
	epoll_ctl(epfd, EPOLL_CTL_DEL, user_data->confd, 0);
	close(user_data->confd);
	--http_conn::m_user_count;
}

//初始化一个新的http连接，并且需为其初始化一个timer
void WebServer::init_http_conn(int connfd, struct sockaddr_in client_address)
{
	m_users_conn[connfd].init(connfd, client_address, m_root, m_connTrigmode, m_close_log);
	//初始化client_data数据
	//创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
	Timer *timer = new Timer(time(NULL) + 3*TIMESLOT, cb_func, &m_users_timer[connfd]);

	m_users_timer[connfd].timer = timer;
	m_users_timer[connfd].address = client_address;
	m_users_timer[connfd].confd = connfd;

	m_timerList->addTimer(timer);
}

void WebServer::adjust_Timer(Timer *timer)
{
	timer->setExpireTime(time(NULL) + 3*TIMESLOT);
	m_timerList->adjust(timer);
	LOG_INFO("%s", "adjust timer once");
}

void WebServer::del_Timer(Timer *timer, int confd)
{
	timer->callback(m_epollfd);
	m_timerList->delTimer(timer);
	LOG_INFO("close fd %d", m_users_timer[confd].confd);
}

void WebServer::dealclientconnection()
{
	struct sockaddr_in client_address;
	socklen_t client_addrlength = sizeof(client_address);
	if (0 == m_listenTrigmode){//监听套接字水平触发
		int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
		if (connfd < 0){
			LOG_ERROR("%s:errno is:%d", "accept error", errno);
			return;
		}
		if (http_conn::m_user_count >= MAX_FD){
			char info[] = "Internal server busy";
			send(connfd, info, sizeof(info), 0);
			LOG_ERROR("%s", info);
			return;
		}
		init_http_conn(connfd, client_address);
	}
	else{//监听套接字边沿触发
		while (1){
			int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
			if (connfd < 0){
				LOG_ERROR("%s:errno is:%d", "accept error", errno);
				break;
			}
			if (http_conn::m_user_count >= MAX_FD){
				char info[] = "Internal server busy";
				send(connfd, info, sizeof(info), 0);
				LOG_ERROR("%s", info);
				break;
			}
			init_http_conn(connfd, client_address);
		}
		return;
	}
}

//m_pipefd[0] == EPOLLIN触发此函数
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
	char signals[1024];
	int ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
	if (ret == -1)
		return false;
	else if (ret == 0)
		return false;
	else{
		for (int i = 0; i < ret; ++i){
			switch (signals[i])
			{
				case SIGALRM:
				{
					timeout = true;
					break;
				}
				case SIGTERM:
				{
					stop_server = true;
					break;
				}
			}
		}
	}
	return true;
}

void WebServer::dealwithread(int sockfd)
{
	Timer *timer = m_users_timer[sockfd].timer;

	//reactor
	if (1 == m_actor_model){
		if (timer){
			adjust_Timer(timer);
		}

		//若监测到读事件，将该事件放入请求队列
		m_threadpool->append(m_users_conn + sockfd, 0);//0为读事件，1为写事件
		while (true){
			if (1 == m_users_conn[sockfd].improv){
				if (1 == m_users_conn[sockfd].timer_flag){//工作线程read_once()读失败
					del_Timer(timer, sockfd);
					m_users_conn[sockfd].timer_flag = 0;
				}
				m_users_conn[sockfd].improv = 0;
				break;
			}
		}
	}
	else{
		//proactor
		if (m_users_conn[sockfd].read_once()){
			LOG_INFO("deal with the client(%s)", inet_ntoa(m_users_conn[sockfd].get_address()->sin_addr));

			//若监测到读事件，将该事件放入请求队列
			m_threadpool->append_p(m_users_conn + sockfd);

			if (timer){
				adjust_Timer(timer);
			}
		}
		else{
			del_Timer(timer, sockfd);
		}
	}
}

void WebServer::dealwithwrite(int sockfd)
{
	Timer *timer = m_users_timer[sockfd].timer;
	//reactor
	if (1 == m_actor_model){
		if (timer){
			adjust_Timer(timer);
		}

		m_threadpool->append(m_users_conn + sockfd, 1);

		while (true){
			if (1 == m_users_conn[sockfd].improv){
				if (1 == m_users_conn[sockfd].timer_flag){
					del_Timer(timer, sockfd);
					m_users_conn[sockfd].timer_flag = 0;
				}
				m_users_conn[sockfd].improv = 0;
				break;
			}
		}
	}
	else{
		//proactor
		if (m_users_conn[sockfd].write()){
			LOG_INFO("send data to the client(%s)", inet_ntoa(m_users_conn[sockfd].get_address()->sin_addr));

			if (timer){
				adjust_Timer(timer);
			}
		}
		else{
			del_Timer(timer, sockfd);
		}
	}
}

void WebServer::eventLoop()
{
	bool timeout = false;
	bool stop_server = false;

	while (!stop_server){
		int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUM, -1);
		if (number < 0 && errno != EINTR){
			LOG_ERROR("%s", "epoll failure");
			break;

		}

		for (int i = 0; i < number; i++){
			int sockfd = m_events[i].data.fd;

			//处理新到的客户连接
			if (sockfd == m_listenfd){
				dealclientconnection();
			}
			else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
				//服务器端关闭连接，移除对应的定时器
				Timer *timer = m_users_timer[sockfd].timer;
				del_Timer(timer, sockfd);
			}
			//处理信号
			else if ((sockfd == m_pipefd[0]) && (m_events[i].events & EPOLLIN)){
				bool flag = dealwithsignal(timeout, stop_server);
				if (flag == false)
					LOG_ERROR("%s", "dealclientdata failure");
			}
			//处理客户连接上接收到的数据
			else if (m_events[i].events & EPOLLIN){
				dealwithread(sockfd);
			}
			else if (m_events[i].events & EPOLLOUT){
				dealwithwrite(sockfd);
			}
		}
		if (timeout){
			m_timerList->tick();
			alarm(TIMESLOT);
			LOG_INFO("%s", "timer tick");
			timeout = false;
		}
	}
}
