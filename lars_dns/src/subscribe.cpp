#include "subscribe.h"


extern tcp_server* server;

SubscribeList* SubscribeList::_instance = NULL;
pthread_once_t SubscribeList::_once = PTHREAD_ONCE_INIT;

SubscribeList::SubscribeList()
{
	pthread_mutex_init(&_book_list_lock, NULL);
	pthread_mutex_init(&_push_list_lock, NULL);
}


//订阅功能
void SubscribeList::subscribe(uint64_t mod, int fd)
{
	//将mod-->fd对应关系加入_book_list中
	pthread_mutex_lock(&_book_list_lock);

	_book_list[mod].insert(fd);

	pthread_mutex_unlock(&_book_list_lock);
}

//取消订阅功能
void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
	//将mod-->fd对应关系从_book_list中删除
	pthread_mutex_lock(&_book_list_lock);
	if (_book_list.find(mod) != _book_list.end())
	{
		_book_list[mod].erase(fd);//将fd从set中删除
		if (_book_list[mod].empty())
		{
			//删除map的一对数据
			_book_list.erase(mod);
		}

	}

	pthread_mutex_unlock(&_book_list_lock);
}


void SubscribeList::make_publish_map(listen_fd_set& online_fds, publish_map& need_publish)
{
	publish_map::iterator it;

	pthread_mutex_lock(&_push_list_lock);

	//遍历_push_list找到online_fds匹配的数据，放到need_publish中
	for (it = _push_list.begin(); it != _push_list.end(); ++it)
	{
		//it->first -->fd
		//it->second -->modid/cmdid集合
		if (online_fds.find(it->first) != online_fds.end())
		{
			//当前这个fd是在线的
			//错误的 浅拷贝错误
			//need_publish[it->first] = _push_list[it->first];//两个modid/cmdid的set的赋值

			//深拷贝
			hash_set<uint64_t>::iterator st;
			for (st = _push_list[it->first].begin(); st != _push_list[it->first].end(); ++st)
			{
				need_publish[it->first].insert(*st);
			}
			//从当前_push_list中删掉
			//_push_list.erase(it);
		}
	}
	pthread_mutex_unlock(&_push_list_lock);
}

//主动推送任务
void push_change_task(event_loop *loop, void *args)
{
	SubscribeList *subscribe = (SubscribeList*)args;

	//1 获取全部在线的客户端
	listen_fd_set online_fds;
	loop->get_listen_fds(online_fds);

	//2 从subscribe中的_push_list中 找到与online_fds集合匹配的fd，放到一个新的publish_map中
	publish_map need_publish;//真正需要发布的订单
	subscribe->make_publish_map(online_fds, need_publish);

	//3 依次从need_publish中取出数据 发送给对应的客户电话
	publish_map::iterator it;
	for (it = need_publish.begin(); it != need_publish.end(); ++it)
	{
		//it->first--> 在线的fd
		//it->second--> 当前fd所订阅的mod中被修改过的mod集合
		int fd = it->first;
		hash_set<uint64_t>::iterator st;
		//遍历fd对应的modid/cmdid的集合
		for (st = it->second.begin(); st != it->second.end(); ++st)
		{
			//一个mod信息
			int modid = int((*st) >> 32);
			int cmdid = int(*st);

			//组装回复的proto消息
			lars::GetRouteResponse rsp;

			rsp.set_modid(modid);
			rsp.set_cmdid(cmdid);

			//通过route查询对应host信息 进行封装
			host_set hosts = Route::instance()->get_hosts(modid, cmdid);
			for (host_set_it hit = hosts.begin(); hit != hosts.end(); ++hit)
			{
				uint64_t ip_port_pair = *hit;
				lars::HostInfo host_info;
				host_info.set_ip((uint32_t)(ip_port_pair >> 32));
				host_info.set_port((int)(ip_port_pair));

				//将host_info加入rsp
				rsp.add_host()->CopyFrom(host_info);
			}

			//将rsp发送给对应fd
			std::string responseString;
			rsp.SerializeToString(&responseString);

			//通过fd得到链接对象
			net_connection* conn = tcp_server::conns[fd];
			if (conn != NULL)
			{
				conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
			}

			SubscribeList::instance()->get_push_list()->erase(fd);
		}
	}
}

//发布功能
//输入形参：是那些modid被修改了，被修改的模块对应的fd就应该被发布，收到新的modid/cmdid的结果
void SubscribeList::publish(std::vector<uint64_t>& change_mods)
{
	
	pthread_mutex_lock(&_book_list_lock);
	pthread_mutex_lock(&_push_list_lock);

	std::vector<uint64_t>::iterator it;
	for (it = change_mods.begin(); it != change_mods.end(); ++it)
	{
		uint64_t mod = *it;

		if (_book_list.find(mod) != _book_list.end())
		{
			//当前被修改的mod在订阅清单中
			//将已经订阅当前mod的fd加入到_push_list中
			hash_set<int>::iterator fds_it;
			for (fds_it = _book_list[mod].begin(); fds_it != _book_list[mod].end(); ++fds_it)
			{
				//fds_it就是指向一个已经订阅当前mod客户端fd文件描述符
				int fd = *fds_it;
				_push_list[fd].insert(mod);
			}
		}
	}
	
	pthread_mutex_unlock(&_push_list_lock);
	pthread_mutex_unlock(&_book_list_lock);

	//通知server让server给_push_list中的每个fd发送新的modid/cmdid对应主机信息 回执消息
	 server->get_th_pool()->send_task(push_change_task, this);
}
