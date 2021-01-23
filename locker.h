/*************************************************************************
	> File Name: locker.h
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年01月21日 星期四 18时27分41秒
 ************************************************************************/

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class locker{
public:
	locker(pthread_mutexattr_t *mutexattr = nullptr){
		int ret = pthread_mutex_init(&mutex, mutexattr);
		if(ret != 0)
			throw std::exception();
	}
	
	bool lock(){
		return pthread_mutex_lock(&mutex) == 0;
	}

	bool unlock(){
		return pthread_mutex_unlock(&mutex) == 0;
	}

	~locker(){
		pthread_mutex_destroy(&mutex);
	}

private:
	pthread_mutex_t mutex;	
};

class sem{
public:
	sem(int shared = 0, unsigned int value = 0){
		int ret = sem_init(&m_sem, shared, value);
		if(ret != 0)
			throw std::exception();
	}

	bool post(){
		return sem_post(&m_sem) == 0;
	}

	bool wait(){
		return sem_wait(&m_sem) == 0;
	}

	~sem(){
		sem_destroy(&m_sem);
	}
private:
	sem_t m_sem;
};

class cond{
public:
	cond(pthread_condattr_t *condattr){
		int ret = pthread_cond_init(&m_cond, condattr);
		if(ret != 0)
			throw std::exception();
	}	

	bool wait(pthread_mutex_t *mutex){
		return pthread_cond_wait(&m_cond, mutex) == 0;
	}

	bool signal(){
		return pthread_cond_signal(&m_cond) == 0;
	}

	~cond(){
		pthread_cond_destroy(&m_cond);
	}
private:
	pthread_cond_t m_cond;
};
