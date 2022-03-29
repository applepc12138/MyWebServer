/*************************************************************************
	> File Name: lst_timer.cpp
	> Author: ggboypc12138
	> Mail: lc1030244043@outlook.com 
	> Created Time: 2021年03月14日 星期日 19时59分21秒
 ************************************************************************/

#include"lst_timer.h"

void TimerList::addTimer(Timer *timer)
{
	if(timer == nullptr)
		return;

	if(head == nullptr){
		head = tail = timer;
	}
	else if(timer->m_expiretime < head->m_expiretime){
		timer->m_next = head;
		head->m_pre = timer;
		head = timer;
	}
	else{
		Timer *cur = head;
		while(cur){
			if(cur->m_expiretime > timer->m_expiretime){
				timer->m_next = cur;
				timer->m_pre = cur->m_pre;
				cur->m_pre->m_next = timer;
				cur->m_pre = timer;
				return;
			}
			cur = cur->m_next;
		}
		tail->m_next = timer;
		timer->m_pre = tail;
		tail = timer;
	}
}

void TimerList::delTimer(Timer *timer)
{
	if(timer == nullptr)
		return;
	
	if(head == tail && head == timer){
		delete timer;
		head = tail = nullptr;
	}
	else if(head == timer){
		head = head->m_next;
		head->m_pre = nullptr;
		delete timer;
	}
	else if(tail == timer){
		tail = tail->m_pre;
		tail->m_next = nullptr;
		delete timer;
	}
	else{
		timer->m_next->m_pre = timer->m_pre;
		timer->m_pre->m_next = timer->m_next;
		delete timer;
	}
}

void TimerList::adjust(Timer *timer)
{
	if(timer == nullptr || head == tail)
		return;
	
	Timer *tmp = timer->m_next;
	if(tmp == nullptr || timer->m_expiretime <= tmp->m_expiretime)
		return;
	if(head == timer){
		head = head->m_next;
		head->m_pre = nullptr;
		timer->m_next = timer->m_pre = nullptr;
		addTimer(timer);
	}
	else{
		timer->m_next->m_pre = timer->m_pre;
		timer->m_pre->m_next = timer->m_next;
		timer->m_pre = timer->m_next = nullptr;
		addTimer(timer);
	}
}

void TimerList::tick()
{
	if(head == nullptr)
		return;
	time_t cur_time = time(NULL);
	Timer *cur = head;
	Timer *tmp = nullptr;
	while(cur){
		if(cur->m_expiretime <= cur_time){
			cur->call_back(m_epollfd, cur->m_userdata);
			tmp = cur;
			cur = cur->m_next;
			delTimer(tmp);			
		}
		else 
			break;
	}
	if(cur == nullptr)
		head = tail = nullptr;
	else{
		head = cur;
		head->m_pre = nullptr;
	}
}
