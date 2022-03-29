/*************************************************************************
	> File Name: sql_connection_pool.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月17日 星期三 13时13分28秒
 ************************************************************************/

#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H 

#include<stdio.h>
#include<cstring>
#include<string>
#include<list>
#include<error.h>
#include<mysql/mysql.h>
#include"../locker.h"
#include"../log/log.h"

class connection_pool 
{
public:
	static connection_pool *get_instance();
	void init(std::string url, std::string user, std::string passWord, std::string dataBaseName, int port, int maxConn, int close_log);

public:
	MYSQL *getConnection();
	bool releaseConnection(MYSQL *conn);
	int getFreeConnNum();
	void destroyPool();
private:
	connection_pool()
	{
		m_curConn = 0;
		m_freeConn = 0;
	}
	~connection_pool()//???
	{
		destroyPool();
	}
private:
	int m_maxConn;
	int m_curConn;
	int m_freeConn;

	locker m_mutex;
	std::list<MYSQL *> m_connList;//连接池
	sem m_reserve;

	std::string m_url;
	std::string m_usr;
	std::string m_password;
	std::string m_dataBaseName;
	int m_port;
	bool m_close_log;
};

//数据库连接的获取和释放通过RAII机制封装
class connectionRAII
{
public:
	connectionRAII(MYSQL **conn, connection_pool *connpool);
	~connectionRAII();
private:
	MYSQL *connRAII;
	connection_pool *poolRAII;
};

#endif
