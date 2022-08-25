#pragma once
#include <ext/hash_map>
#include <ext/hash_set>
#include "event_base.h"
#include <sys/epoll.h>
#include <vector>

#define MAXEVENTS 10

//map: key:fd   value:io_event
typedef __gnu_cxx::hash_map<int, io_event> io_event_map;
typedef __gnu_cxx::hash_map<int, io_event>::iterator io_event_map_it;

//set: value:fd 正在监听的fd的集合
typedef __gnu_cxx::hash_set<int> listen_fd_set;

//task 一般任务的回调函数类型
typedef void (*task_func)(event_loop* loop, void *args); 

class event_loop
{
public:
	//构造函数
	event_loop();

	//阻塞循环监听事件，并处理
	void event_process();

	//添加一个IO事件到event_loop中
	void add_io_event(int fd, io_callback *proc, int mask, void *args);

	//删除一个IO事件 从event_loop中
	void del_io_event(int fd);

	//删除一个IO事件的某个触发条件（EPOLLIN/EPOLLOUT）
	void del_io_event(int fd, int mask);

	//针对异步task任务的方法
	//添加一个task任务到_ready_tasks中
	void add_task(task_func func, void *args);

	//执行全部task里面的任务
	void execute_ready_tasks();

	//获取当前event_loop中正在监听的fd集合
	void get_listen_fds(listen_fd_set &fds)
	{
		fds = listen_fds;
	}
private:
	int _epfd; //通过epoll_create来创建

	//当前event_loop监控的fd和对应事件的关系
	io_event_map _io_evs;

	//记录当前event_loop都有哪些fd正在监听 epoll_wait()正在等待哪些fd触发转态
	//作用是便于将当前的服务器可以主动发送消息给客户端	
	//集合里的fd 表示在线的fd
	listen_fd_set listen_fds;

	//每次epoll_wait返回触发，所返回的被激活的事件集合
	struct epoll_event _fired_evs[MAXEVENTS];

	//针对异步task任务的属性
	typedef std::pair<task_func, void*> task_func_pair;
	//目前已经就绪的全部task任务集合
	std::vector<task_func_pair> _ready_tasks;
};
