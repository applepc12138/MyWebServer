/*************************************************************************
	> File Name: http_conn.cpp > Author: ggboypc12138 > Mail: lc1030244043@outlook.com 
	> Created Time: 2021年01月23日 星期六 15时15分44秒
 ************************************************************************/

#include "./http_conn.h"
#include <mysql/mysql.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "../util.h"

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;//????
std::map<std::string, std::string> users;//????

int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

void http_conn::init()
{
	m_mysql = nullptr;
	bytes_to_send = 0;
	bytes_have_send = 0;
	m_check_state = CHECK_STATE_REQUESTLINE;

	// m_method = ;
	m_url = nullptr;
	m_version = nullptr;
	m_host = nullptr;
	m_linger = false;
	m_content_length = 0;

	m_readed_idx = 0;
	m_checked_idx = 0;
	m_start_line_idx = 0;

	m_writed_idx = 0;

	cgi = 0;
	m_state = 0;
	timer_flag = 0;
	improv = 0;

	memset(m_readed_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
		int close_log)
{
	m_socketfd = sockfd;
	m_address = addr;

	utils::addfd(m_epollfd, m_socketfd, EPOLLIN, TRIGMode, true);
	++m_user_count;

	m_doc_root = root;
	m_TRIGMode = TRIGMode;
	m_close_log = close_log;
	init();
}

//关闭连接，客户端数量减一
void http_conn::close_conn(bool real_close)
{
	if(real_close && (m_socketfd != -1)){
		printf("close %d\n", m_socketfd);
		utils::removefd(m_epollfd, m_socketfd);
		m_socketfd = -1;
		--m_user_count;
	}
}

void http_conn::initmysql_result(connection_pool *connPool)
{
	MYSQL *mysql = NULL;
	connectionRAII sqlConn(&mysql, connPool);
	//从连接池中取出的连接用户名、密码和数据库名由连接池初始化时指定，但是表名固定为user
	if(mysql_query(mysql, "SELECT username,passwd FROM user"))
		LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
	//将表中结果集复制到result指向的内存里
	MYSQL_RES *result = mysql_store_result(mysql);
	//int num_fields = mysql_num_fields(result);
	//MYSQL_FIELD *fields = mysql_fetch_fields(result);
	while(MYSQL_ROW row = mysql_fetch_row(result)){
		std::string t1(row[0]);
		std::string t2(row[1]);
		users[t1] = t2;
	}
}
	
//读取客户端(浏览器)发送的请求报文，直到无数据可读或对方关闭连接，数据读取到m_readed_buffer中
bool http_conn::read_once()
{
	if(m_readed_idx >= READ_BUFFER_SIZE)
		return false;
	
	int readed = 0;
	while(true){
		readed = recv(m_socketfd, m_readed_buf + readed, 
				READ_BUFFER_SIZE - m_readed_idx, 0);
		//当socketfd设置为非阻塞时，无数据recv直接返回-1，errno被置为EAGAIN或EWOULDBLOCK
		if(readed == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK)	
				break;
			return false;
		}
		else if(readed == 0){//客户端关闭连接
			return false;//???
		}
		m_readed_idx += readed;
	}
	return true;
}

http_conn::LINE_STATUS http_conn::parse_line()
{
	char temp;
	for( ; m_checked_idx < m_readed_idx; ++m_checked_idx){
		temp = m_readed_buf[m_checked_idx];
		if(temp == '\r'){
			if(m_checked_idx + 1 == m_readed_idx)
				return LINE_OPEN;
			else if(m_readed_buf[m_checked_idx+1] == '\n'){
				m_readed_buf[m_checked_idx++] = '\0';
				m_readed_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			else 
				return LINE_BAD;
		}
		if(temp == '\n'){
			if(m_checked_idx > 0 && m_readed_buf[m_checked_idx-1] == '\r'){
				m_readed_buf[m_checked_idx-1] = '\0';
				m_readed_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}	

http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
	//以 GET  http://hello.html http/1.1 为例
	m_url = strpbrk(text, " \t");
	if(m_url == nullptr)
		return BAD_REQUEST;
	*m_url++ = '\0';
	if(strcasecmp(text, "GET") == 0)
		m_method = GET;
	else if(strcasecmp(text, "POST") == 0){
		m_method = POST;
		cgi = 1;
	}
	else 
		return BAD_REQUEST;
	
	//m_url已经跳过了第一个空格或\t字符，但不知道后面是否还有
	//继续跳过空格\t字符，直到指向请求资源的第一个字符
	m_url += strspn(m_url, " \t");
	
	m_version = strpbrk(m_url, " \t");
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");

	if(strcasecmp(m_version, "HTTP/1.1") != 0)
		return BAD_REQUEST;

	if(strncasecmp(m_url, "http://", 7) == 0){
		m_url += 7;
	}

	if(strncasecmp(m_url, "https://", 8) == 0){
		m_url += 8;
	}
	m_url = strchr(m_url, '/');

	if(!m_url || m_url[0] != '/')//???
		return BAD_REQUEST;
	if(strlen(m_url) == 1)//url格式为 http:///
		strcat(m_url, "judge,html");
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}	

//解析http请求的一个首部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
	if(text[0] == '\0'){
		if(m_content_length == 0)
			return GET_REQUEST;
		m_check_state = CHECK_STATE_CONTENT;
		return NO_REQUEST;
	}
	
	if(strncasecmp(text, "Connection:", 11) == 0){
		text += 11;
		text += strspn(text, " \t");
		if(strcasecmp(text, "keep-alive") == 0)
			m_linger = true;
	}
	else if(strncasecmp(text, "Content-length:", 15) == 0){
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	else if(strncasecmp(text, "Host:", 5) == 0){
		text += 5;
		text += strspn(text, " \t");
		m_host = text;//???
	}
	else{ 
		printf("oop!unknow header: %s\n", text);
		return BAD_REQUEST;
	}
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
	if(m_readed_idx < (m_content_length + m_checked_idx))	
		return NO_REQUEST;//???
	text[m_content_length] = '\0';//
	m_string = text;
	return GET_REQUEST;
}
	
http_conn::HTTP_CODE http_conn::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char *text = nullptr;

	while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK)){
		text = get_line();	
		m_start_line_idx = m_checked_idx;
		
		switch(m_check_state){
			case CHECK_STATE_REQUESTLINE:
			{
				ret = parse_request_line(text);//返回值有两种
				if(ret == BAD_REQUEST)
					return ret;
				// m_check_state = CHECK_STATE_HEADER;
				break;
			}
			case CHECK_STATE_HEADER:
			{
				ret = parse_headers(text);//返回值有三种
				if(ret == GET_REQUEST)				
					return do_request();
				else if(ret == BAD_REQUEST)
					return ret;
				break;
			}
			case CHECK_STATE_CONTENT://POST类型请求报文会跳转到此处
			{
				ret = parse_content(text);//返回值有两种
				if(ret == GET_REQUEST)
					return do_request();
				//NO_REQUEST
				line_status = LINE_OPEN;//???
				break;
			}
			default:
				return INTERNAL_ERROR;
		}
	}
	return NO_REQUEST;
}
	
http_conn::HTTP_CODE http_conn::do_request()
{
	strcpy(m_real_file, m_doc_root);
	int len = strlen(m_doc_root);
	
	const char *p = strrchr(m_url, '/');
	
	//登录校验和注册校验
	//m_url = "/2CGISQL.cgi" 或 m_url = "/3CGISQL.cgi"
	if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')){
		char *url_real = (char *)malloc(sizeof(char)*200);
		strcpy(url_real, "/");
		strcat(url_real, m_url + 2);
		strncpy(m_real_file + len, url_real, FILENAME_LEN - len -1);
		free(url_real);

		//将用户名和密码从m_string中提取出来
		//user=123&passwd=123
		char name[32], passwd[32];
		int i;
		for(i = 5; m_string[i] != '&'; ++i)
			name[i - 5] = m_string[i];
		name[i - 5] = '\0';

		int j = 0;
		for(i = i + 10; m_string[i] != '\0'; ++i, ++j)
			passwd[j] = m_string[i];
		passwd[j] = '\0';

		//注册校验 
		//注册成功跳转到log.html,即登录界面
		//注册失败跳转到registerError.html,即注册失败页面
		if(*(p + 1) == '3'){
			char *sql_insert = (char *)malloc(sizeof(char)*200);
			//INTSERT INTO user(username, passwd) VALUES('123', '123')
			strcpy(sql_insert, "INTSERT INTO user(username, passwd) VALUES(");
			strcat(sql_insert, "'");
			strcat(sql_insert, name);
			strcat(sql_insert, "', '");
			strcat(sql_insert, passwd);

			if(users.find(name) == users.end()){//注册时数据库中无重名
				m_lock.lock();
				int res = mysql_query(m_mysql, sql_insert);
				users.insert(std::pair<std::string, std::string>(name, passwd));
				m_lock.lock();
				if(res == 0)
					strcpy(m_url, "/log.html");
				else 
					strcpy(m_url, "/registerError.html");
			}
			else//注册时有重名
				strcpy(m_url, "/registerError.html");
		}
		//登录校验 
		//验证成功跳转到welcome.html,即资源请求成功页面
		//验证失败跳转到logError.html,即登录失败页面
		else if(*(p + 1) == '2'){
			if((users.find(name) != users.end() && users[name] == passwd))
				strcpy(m_url, "/welcome.html");
			else 
				strcpy(m_url, "/logError.html");
		}
	}
	
	//
	const char *s = nullptr;
	if(*(p+1) == '0'){
		s = "/register.html";
		strncpy(m_real_file + len, s, strlen(s));
	}
	else if(*(p+1) == '1'){//post请求，跳转到judge.html,即欢迎访问页面/
		s = "/log.html";
		strncpy(m_real_file + len, s, strlen(s));
	}
	else if(*(p+1) == '5'){//post请求，跳转到picture.html,即图片请求页面
		s = "/picture.html";
		strncpy(m_real_file + len, s, strlen(s));
	}
	else if(*(p+1) == '6'){//post请求，跳转到video.html
		s = "/video.html";
		strncpy(m_real_file + len, s, strlen(s));
	}
	else if(*(p+1) == '7'){//post请求，跳转到fans.html
		s = "/fans.html";
		strncpy(m_real_file + len, s, strlen(s));
	}
	else//跳转到欢迎界面，此时m_url = judge.html
		strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
	
	if(stat(m_real_file, &m_file_stat) < 0)
		return NO_RESOURCE;
	//判断文件类型，如果是目录，则返回BAD_REQUEST
	if(S_ISDIR(m_file_stat.st_mode))
		return BAD_REQUEST;
	//以只读方式获取文件描述符，通过mmap将该文件映射到内存
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char *)mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	
	close(fd);
	return FILE_REQUEST;
}

bool http_conn::add_response(const char *format, ...)
{
	if(m_writed_idx >= WRITE_BUFFER_SIZE)
		return false;

	va_list arg_list;
	va_start(arg_list, format);
	
	int len = vsnprintf(m_write_buf + m_writed_idx, 
			WRITE_BUFFER_SIZE - m_writed_idx - 1, format, 
			arg_list);
	if(len >= WRITE_BUFFER_SIZE - m_writed_idx - 1){
		va_end(arg_list);
		return false;
	}	

	m_writed_idx += len;
	va_end(arg_list);
	return true;
}

bool http_conn::add_status_line(int status, const char *title){
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
	add_content_length(content_len);
	add_linger();
	add_blank_line();
	return true;
}

bool http_conn::add_content_length(int content_len)
{
	return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type()
{
	return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger()
{
	return add_response("Connection:%s\r\n", 
			(m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
	return add_response("\r\n");
}

bool http_conn::add_content(const char *text)
{
	return add_response("%s", text);
}

void http_conn::unmap()
{
	if(m_file_address){
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}

//process_write()并没有真正将数据发送至客户端，
//只是将数据写入发送缓冲区中,真正的发送动作由write()完成
bool http_conn::process_write(HTTP_CODE ret)
{
	switch(ret)
	{
		case INTERNAL_ERROR:
		{
			add_status_line(500, error_500_title);
			add_headers(strlen(error_500_form));
			add_content(error_500_form);
			break;
		}
		case BAD_REQUEST:
		{
			add_status_line(404, error_404_title);
			add_headers(strlen(error_404_form));
			add_content(error_404_form);
			break;
		}
		case FORBIDDEN_REQUEST:
		{
			add_status_line(403, error_403_title);
			add_headers(strlen(error_403_form));
			add_content(error_403_form);
			break;
		}
		case FILE_REQUEST:
		{
			add_status_line(200, ok_200_title);
			if(m_file_stat.st_size > 0){
				add_headers(m_file_stat.st_size);
				m_iv[0].iov_base = m_write_buf;
				m_iv[0].iov_len = m_writed_idx;

				m_iv[1].iov_base = m_file_address;
				m_iv[1].iov_len = m_file_stat.st_size;
				m_iv_count = 2;
				bytes_to_send = m_writed_idx + m_file_stat.st_size;
				return true;
			}
			else{//请求资源大小为0，返回空白html文件
				const char *strs = "<html><body></body></html>";
				add_headers(strlen(strs));
				add_content(strs);
				//return true;
			}
		}
		default:
			return false;
	}
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_writed_idx;
	m_iv_count = 1;
	return true;
}

//服务器子线调用process()通过process_write()向m_write_buf写入响应报文，
//随后注册EPOLLOUT事件，主线程检测写事件并通过write()将响应报文发至浏览器端。
bool http_conn::write()
{
	int data_sended = 0;
	int newadd = 0;

	//???
	if(bytes_to_send == 0){
		utils::modfd(m_epollfd, m_socketfd, EPOLLIN, 1);		
		init();
		return true;
	}

	while(true){
		data_sended = writev(m_socketfd, m_iv, m_iv_count); 

		if(data_sended > 0){
			bytes_have_send += data_sended;
			newadd = bytes_have_send - m_writed_idx;//???
		}
		else if(data_sended <= 0){
			if(errno == EAGAIN){//发送缓冲区已满
				if(bytes_have_send >= m_iv[0].iov_len){//iovec[0]数据已发送完
					m_iv[0].iov_len = 0;
					m_iv[1].iov_base = m_file_address + newadd;
					m_iv[1].iov_len = bytes_to_send;
				}
				else{//iovec[0]还有部分数据未发送
					m_iv[0].iov_base = m_write_buf + bytes_to_send;
					m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
				}
				utils::modfd(m_epollfd, m_socketfd, EPOLLOUT, 1);
				return true;
			}
			unmap();
			return false;
		}
		bytes_to_send -= data_sended;

		if(bytes_to_send <= 0){
			unmap();
			utils::modfd(m_epollfd, m_socketfd, EPOLLIN, 1);
			if(m_linger){
				init();
				return true;
			}
			else
				return false;
		}
	}
}

//process()不涉及数据的发送(write())与接收(read_once())，只进行数据的分析与处理
void http_conn::process()
{
	//处理过程：
	//此时已通过read_once()将数据接收到接收缓冲区m_readed_buf中
	//process_read()对已接收数据进行分析和响应(do_request)
	HTTP_CODE read_ret = process_read();
	if(read_ret == NO_REQUEST){
		utils::modfd(m_epollfd, m_socketfd, EPOLLIN, m_TRIGMode);
		return;
	}
	//将需发送至客户端的数据写入发送缓冲区
	bool write_ret = process_write(read_ret);
	if(!write_ret)
		close_conn();
	//添加监听写事件
	utils::modfd(m_epollfd, m_socketfd, EPOLLOUT, m_TRIGMode);
}
