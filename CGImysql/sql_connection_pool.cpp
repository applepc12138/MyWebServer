/*************************************************************************
	> File Name: sql_connection_pool.cpp
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月17日 星期三 13时13分44秒
 ************************************************************************/

#include"sql_connection_pool.h"
#include <mysql/mysql.h>

connection_pool *connection_pool::get_instance()
{
	static connection_pool instance;
	return &instance;
}

void connection_pool::init(std::string url, std::string user, std::string passWord, std::string dataBaseName, int port, int maxConn, int close_log)
{
	m_url = url;
	m_port = port;
	m_usr = user;
	m_password = passWord;
	m_dataBaseName = dataBaseName;

	//创建maxConn个数据库连接
	for(int i = 0; i < maxConn; ++i){
		MYSQL *conn = nullptr;
		//初始化MYSQL结构体
		conn = mysql_init(conn);
		
		if(conn == NULL){
			LOG_ERROR("MYSQL Error");
			exit(1);
		}

		//尝试与运行在主机host上的MYSQL数据库引擎建立连接
		conn = mysql_real_connect(conn, m_url.c_str(), m_usr.c_str(),
				m_password.c_str(), m_dataBaseName.c_str(), m_port, NULL, 0); 
		if(conn == NULL){
			LOG_ERROR("MYSQL Error");
			exit(1);
		}
		m_connList.push_back(conn);
		++m_freeConn;
	}
	m_reserve = sem(0, m_freeConn);
	m_maxConn = m_freeConn;
}

MYSQL *connection_pool::getConnection()
{
	MYSQL *conn = nullptr;
	if(m_connList.size() == 0)
		return NULL;

	m_reserve.wait();

	m_mutex.lock();
	conn = m_connList.front();
	m_connList.pop_front();
	--m_freeConn;
	++m_curConn;
	m_mutex.unlock();

	return conn;
}

bool connection_pool::releaseConnection(MYSQL *conn)
{
	if(conn == NULL)
		return false;

	m_mutex.lock();
	m_connList.push_back(conn);
	++m_freeConn;
	--m_curConn;
	m_mutex.unlock();
	
	m_reserve.post();
	return true;
}
	
void connection_pool::destroyPool()
{
	m_mutex.lock();

	if(m_connList.size() > 0){
		for(auto i = m_connList.begin(); i != m_connList.end(); ++i){
			MYSQL *conn = *i;
			mysql_close(conn);
		}
		m_curConn = 0;
		m_freeConn = 0;
		m_connList.clear();
	}
	m_mutex.unlock();
}

int connection_pool::getFreeConnNum()
{
	return m_freeConn;
}

connectionRAII::connectionRAII(MYSQL **sql, connection_pool *connpool)
{
	*sql = connpool->getConnection();

	connRAII = *sql;
	poolRAII = connpool;
}

connectionRAII::~connectionRAII()
{
	poolRAII->releaseConnection(connRAII);
}


