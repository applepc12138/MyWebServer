/*************************************************************************
	> File Name: threadpool.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月13日 星期六 20时44分11秒
 ************************************************************************/

#ifndef THREADPOOL_H
#define THREADPOOL_H 

#include <pthread.h>
#include <list>
#include <exception>
#include <type_traits>
#include "locker.h"
#include "./CGImysql/sql_connection_pool.h"

template<typename T>
class threadpool 
{
public:
	threadpool(int actor_model, connection_pool *conn_pool, int thread_num = 0, int max_tasks = 10000);
	~threadpool();
	bool append(T *request, int state);
	bool append_p(T *request);
private:
	static void *worker(void *arg);
	void run();
private:
	int m_thread_num;
	int m_max_tasks;
	pthread_t *m_threads;
	std::list<T *> m_workqueue;
	locker m_queuelock;
	sem m_queuestat;
	connection_pool *m_conn_pool;
	int m_actor_model;
};

template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *conn_pool, int thread_num, int max_tasks):
	m_thread_num(thread_num), m_max_tasks(max_tasks), m_conn_pool(conn_pool), m_actor_model(actor_model)
{
	if(thread_num <= 0 || max_tasks <= 0)
		throw std::exception();
	
	m_threads = new pthread_t[m_thread_num];
	if(m_threads == nullptr)
		throw std::exception();
	
	for(int i = 0; i < m_thread_num; ++i){
		if(pthread_create(&m_threads[i], nullptr, worker, this) != 0){
			delete[] m_threads;
			throw std::exception();
		}
		if(pthread_detach(m_threads[i]) != 0){
			delete[] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
threadpool<T>::~threadpool()
{
	delete[] m_threads;
}

//主线程向任务队列中添加写任务或读任务，state=0为读，1为写
template<typename T>
bool threadpool<T>::append(T *request, int state)
{
	if(!m_queuelock.trylock())
		return false;

	if(m_workqueue.size() >= m_max_tasks){
		m_queuelock.unlock();
		return false;
	}
	request->m_state = state;
	m_workqueue.push_back(request);
	m_queuelock.unlock();
	m_queuestat.post();
	return true;
}

template<typename T>
bool threadpool<T>::append_p(T *request)
{
	m_queuelock.lock();
	if(m_workqueue.size() >= m_max_tasks){
		m_queuelock.unlock();
		return false;
	}
	m_workqueue.push_back(request);
	m_queuelock.unlock();
	m_queuestat.post();
	return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg)
{
	threadpool *pool = (threadpool *)arg;
	pool->run();
	return pool;
}

template<typename T>
void threadpool<T>::run()
{
	while(true){
		m_queuestat.wait();
		m_queuelock.lock();
		if(m_workqueue.empty()){
			m_queuelock.unlock();
			continue;
		}
		T *task = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelock.unlock();
		if(!task)//???
			continue;
		if(m_actor_model == 1){//reactor
			if(task->m_state == 0){//http_conn类中，读为0，写为1
				if(task->read_once()){
					task->improv = 1;//????
					connectionRAII mysqlcon(&task->m_mysql, m_conn_pool);//是否多余????
					task->process();
				}
				else{
					task->improv = 1;
					task->timer_flag = 1;
				}
			}
			else{//写事件
				if(task->write())
					task->improv = 1;
				else{
					task->improv = 1;
					task->timer_flag = 1;
				}
			}
		}
		else{//proactor
			connectionRAII mysqlcon(&task->m_mysql, m_conn_pool);//是否多余????
			task->process();
		}
	}
}

#endif
