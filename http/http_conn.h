/*************************************************************************
	> File Name: http_conn.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年01月23日 星期六 15时15分31秒
 ************************************************************************/
#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H 

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <mysql/mysql.h>
#include"../CGImysql/sql_connection_pool.h"
#include "../locker.h"

class http_conn
{
public:
	static constexpr int FILENAME_LEN = 200;
	static constexpr int READ_BUFFER_SIZE = 200;
	static constexpr int WRITE_BUFFER_SIZE = 200;

	//http请求方法
	enum METHOD
	{
		GET = 0,
		POST,
		HEAD,
		PUT,
		DELETE,
		TRACE,
		OPTIONS,
		CONNECT,
		PATH
	};

	//主状态机处理状态
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};

	//主状态机三种处理状态处理时的返回结果
	enum HTTP_CODE
	{
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};

	//从状态机处理一行的返回结果
	//从状态机的作用是从m_read_buf中确定一个完整的行(http报文一个完整行由\r\n标记)
	enum LINE_STATUS
	{
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN//读取的行不完整
	};

public:
	http_conn(){}
	~http_conn(){}
	
public:
	void init(int socked, const sockaddr_in &addr, char *root, int TRIGMode, int close_log);
	void close_conn(bool real_close = true);
	void process();
	bool read_once();
	bool write();
	sockaddr_in *get_address(){
		return &m_address;
	}

	void initmysql_result(connection_pool *connPool);
	int timer_flag;
	int improv;

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);
	HTTP_CODE parse_request_line(char *text);
	HTTP_CODE parse_headers(char *text);
	HTTP_CODE parse_content(char *text);
	HTTP_CODE do_request();
	//返回请求报文首部需要处理的新一行
	char *get_line() { return m_readed_buf + m_start_line_idx;  }
	LINE_STATUS parse_line();
	void unmap();
	bool add_response(const char *format, ...);
	bool add_content(const char *content);
	bool add_status_line(int status, const char *title);
	bool add_headers(int content_length);
	bool add_content_type();
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int m_epollfd;
	static int m_user_count;
	MYSQL *m_mysql;
	int m_state; //读为0，写为1

private:
	//服务器与客户端通信socket
	int m_socketfd;
	//客户端的socket地址
	sockaddr_in m_address;
	//读缓冲
	char m_readed_buf[READ_BUFFER_SIZE];
	int m_readed_idx;
	int m_checked_idx;
	int m_start_line_idx;
	//写缓冲
	char m_write_buf[WRITE_BUFFER_SIZE];
	int m_writed_idx;

	//主状态机当前所处的状态
	CHECK_STATE m_check_state;
	//请求方法
	METHOD m_method;
	//客户请求的目标文件的完整路径，其内容等于doc_root+m_url,doc_root是网站的根目录
	char m_real_file[FILENAME_LEN];
	//m_url为请求报文中解析出的请求资源，以/开头(/xxx)
	char *m_url;
	//HTTP协议版本号
	char *m_version;
	//主机名
	char *m_host;
	//HTTP请求的消息体长度
	int m_content_length;
	//HTTP请求是否要求保持连接
	bool m_linger;
	//客户请求的目标文件被mmap到内存中的起始位置
	char *m_file_address;
	//客户请求的目标文件的状态
	struct stat m_file_stat;
	//采用writev来执行写操作，所以定义下面两个成员，其中m_iv_conut标识被写内存快的数量
	struct iovec m_iv[2];
	int m_iv_count;
	int cgi;        //是否启用的POST
	char *m_string; //存储请求头数据
	int bytes_to_send;
	int bytes_have_send;
	//网站根目录
	char *m_doc_root;

	//用户名和密码
	std::map<std::string, std::string> m_users;
	int m_TRIGMode;//是否是边沿触发模式
	int m_close_log;

	//char sql_user[100];//此字段无用
	//char sql_passwd[100];//此字段无用
	//char sql_name[100];//此字段无用
};

#endif

