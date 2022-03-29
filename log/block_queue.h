/*************************************************************************
	> File Name: block_queue.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月16日 星期二 13时14分25秒
 ************************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H 
#include<stdlib.h>
#include<pthread.h>
#include<queue>
#include"../locker.h"

//异步日志，将生产者-消费者模型封装为阻塞队列，工作线程将要写的内容push进队列。创建一个(多个?)写线程，写线程从队列中取出内容，写入日志文件。

template<typename T>
class block_queue
{
public:
	block_queue(int maxsize = 1000):m_max_size(maxsize){}
	~block_queue(){}

public:
	//工作线程将日志push进队列
	bool push(const T&item)
	{
		m_mutex.lock();
		if(q.size() >= m_max_size){
			m_cond.signal();
			m_mutex.unlock();
			return false;
		}
		q.push(item);
		m_mutex.unlock();
		return true;
	}
	//写线程从队列中取日志，若队列为空写线程被阻塞
	bool pop(T &value)
	{
		m_mutex.lock();
		while(q.empty()){
			m_cond.wait(m_mutex.get());
		}
		value = q.front();
		q.pop();
		m_mutex.unlock();
		return true;
	}
	bool isfull()
	{
		return m_max_size == q.size();
	}
private:
	int m_max_size;//队列最大容量
	std::queue<T> q;//日志内容
	locker m_mutex;
	cond m_cond;
};

#endif
