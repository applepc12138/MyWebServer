/*************************************************************************
	> File Name: log.cpp
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月16日 星期二 11时50分26秒
 ************************************************************************/

#include"log.h"
#include"block_queue.h"
#include <cstdio>
#include<pthread.h>
#include<cstring>
#include<time.h>
#include<sys/time.h>

//完成日志文件创建、写入方式的判断
//通过单例模式获取唯一的日志类，调用init()方法，初始化生成日志文件，服务器启动后按当前时间创建日志文件，前缀为时间，后缀为自定义文件名，并记录创建日志的时间day和行数count
//file_name可能为单独文件名或绝对路径
bool Log::init(const char*file_name, bool close_log, int log_buf_size, int split_lines, int max_queue_size)
{
	//如果设置了max_queue_size，则设置为异步
	if(max_queue_size > 0){
		m_is_async = true;
		m_log_queue = new block_queue<std::string>(max_queue_size);
		pthread_t tid;
		pthread_create(&tid, NULL, Log::async_write_log, NULL);
	}
	m_close_log = close_log;
	m_log_buf_size = log_buf_size;
	m_buf = new char[m_log_buf_size];
	memset(m_buf, '\0', m_log_buf_size);
	
	//time(NULL)返回自19700101 00:00经过的时间，以秒为单位
	time_t t = time(NULL);
	struct tm *sys_tm = localtime(&t);
	struct tm my_tm = *sys_tm;

	const char *p = strrchr(file_name, '/');
	char log_full_name[256] = {0};

	if(p == NULL){//以时间+file_name作为日志文件名
		snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, 
				my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
	}
	else{//路径+时间+file_name作为日志文件名
		strcpy(m_log_name, p + 1);

		//m_dir_name并非一定会被初始化
		strncpy(m_dir_name, file_name, p - file_name + 1);

		snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", m_dir_name, my_tm.tm_year + 1900, 
				my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
	}
	m_today = my_tm.tm_mday;
	
	m_fp = fopen(log_full_name, "a");
	if(m_fp == NULL)
		return false;
	return true;
	
}

//由主线程和工作线程调用!!!!!!
//完成日志分级、分文件、格式化输出内容
//日志写入前会判断当前day是否为创建日志的时间，行数是否超过最大行限制
//若为创建日志时间，写入日志，否则按当前时间创建新log，更新创建时间和行数
//若行数超过最大行限制，在当前日志的末尾添加count/max_lines为后缀创建新log
//将系统信息格式化后输出：格式化时间+格式化内容
void Log::formatting_write_log(int level, const char *format, ...)
{
	struct timeval now = {0, 0};
	gettimeofday(&now, NULL);
	struct tm my_tm = *(localtime(&now.tv_sec));
	char s[16] = {0};
	
	switch (level)
	{
		case 0:
			strcpy(s, "[debug:]");
			break;
		case 1:
			strcpy(s, "[info]:");
			break;
		case 2:
			strcpy(s, "[warn]:");
			break;
		case 3:
			strcpy(s, "[erro]:");
			break;
		default:
			strcpy(s, "[debug]:");
			break;
	}

	m_mutex.lock();
	++m_count;
	//日志时间不是今天或写入的日志行数超过最大行数
	if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
		fflush(m_fp);
		fclose(m_fp);
		char tail[16] = {0};
		snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
		
		char new_log[256] ={0};//m_dir_name+m_log_name
		if(m_today != my_tm.tm_mday){//日志时间不是今天 
			//按当前时间创建新log，更新创建时间和行数
			snprintf(new_log, 255, "%s%s%s", m_dir_name, tail, m_log_name);
			m_today = my_tm.tm_mday;
			m_count = 0;
		}
		else{//当天日志行数超过了最大行数
			//在当前日志的末尾添加count/max_lines为后缀创建新log
			snprintf(new_log, 255, "%s%s%s.%lld", m_dir_name, tail, m_log_name, m_count/m_split_lines);
		}
	}
	m_mutex.unlock();

	va_list valist;
	va_start(valist, format);
	
	m_mutex.lock();
	//首先写入格式化时间，
	int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d.%06ld %s ", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, 
			my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
	//再写入格式化内容
	int m = vsnprintf(m_buf + n, m_log_buf_size - 1, format, valist);
	m_buf[n + m] = '\n';
	m_buf[n + m + 1] = '\0';
	std::string log_string = m_buf;
	m_mutex.unlock();

	if(m_is_async || !m_log_queue->isfull()){
		m_log_queue->push(log_string);
	}
	else{//由工作线程或主线程直接写入
		m_mutex.lock();
		fputs(m_buf, m_fp);
		m_mutex.unlock();
	}
	va_end(valist);
}

void Log::flush()
{
	m_mutex.lock();
	fflush(m_fp);
	m_mutex.unlock();
}

Log::~Log()
{
	if(m_fp)
		fclose(m_fp);
	if(m_buf)
		delete[] m_buf;
	if(m_log_queue)
		delete m_log_queue;
}
