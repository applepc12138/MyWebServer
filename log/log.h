/*************************************************************************
	> File Name: log.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月16日 星期二 11时50分31秒
 ************************************************************************/

#ifndef LOG_H
#define LOG_H 

#include<stdio.h>
#include<string>
#include<stdarg.h>
#include<pthread.h>
#include"block_queue.h"

//日志类包括以下方法
//公有的实例获取方法
//初始化日志文件方法
//异步日志写入方法，内部调用私有异步方法
//内容格式化方法
//刷新缓冲区
class Log 
{
public:
	static Log *get_intance()
	{
		static Log instance;
		return &instance;
	}
	//注意写法!!!
	//此函数需传递给pthread_create(),所以必须为静态函数；又因为需要访问Log类成员变量，所以需要封装async_write_log
	static void *async_write_log(void *)
	{
		get_intance()->async_write_log();
		return nullptr;
	}

	bool init(const char *file_name, bool close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
	void formatting_write_log(int level, const char *format, ...);
	void flush();
	

private:
	//Log();
	//Log(const Log &)=delete;
	~Log();

	//异步日志写入方法
	void async_write_log()
	{
		std::string single_log;
		while(m_log_queue->pop(single_log))//写线程从阻塞队列中取出要写入的日志后，加锁写入日志文件
		{
			m_mutex.lock();
			fputs(single_log.c_str(), m_fp);
			m_mutex.unlock();
		}
	}
private:
	char m_dir_name[128];//路径名
	char m_log_name[128];//文件名
	int m_split_lines;//日志最大行数
	long long m_count;//日志行数记录
	int m_today;//按天分类，记录当前时间是哪一天
	FILE *m_fp;//打开log的文件指针
	char *m_buf;
	int m_log_buf_size;
	block_queue<std::string> *m_log_queue = nullptr;//阻塞队列
	bool m_is_async;//是否异步
	locker m_mutex;
	bool m_close_log;
};

//对日志等级进行分类，包括BEBUG，INFO，WARN和ERROR四种级别的日志
//这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) if(m_close_log == 0){Log::get_intance()->formatting_write_log(0, format, ##__VA_ARGS__);Log::get_intance()->flush();}
#define LOG_INFO(format, ...) if(m_close_log == 0){Log::get_intance()->formatting_write_log(1, format, ##__VA_ARGS__);Log::get_intance()->flush();}
#define LOG_WARN(format, ...) if(m_close_log == 0){Log::get_intance()->formatting_write_log(2, format, ##__VA_ARGS__);Log::get_intance()->flush();}
#define LOG_ERROR(format, ...) if(m_close_log == 0){Log::get_intance()->formatting_write_log(3, format, ##__VA_ARGS__);Log::get_intance()->flush();}

#endif

