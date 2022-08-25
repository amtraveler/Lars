#include "load_balance.h"
#include "main_server.h"


int load_balance::pull()
{
	//封装一个dns请求包
	lars::GetRouteRequest req;
	req.set_modid(_modid);
	req.set_cmdid(_cmdid);

	//将这个包发送dns_queue即可
	dns_queue->send(req);
	
	status = PULLING;

	printf("modid %d, cmdid %d pulling from dns service!\n", _modid, _cmdid);

	return 0;
}

//根据Dns service远程返回的主机结果，更新自己的host_map表
void load_balance::update(lars::GetRouteResponse &rsp)
{
	long current_time  = time(NULL);

	std::set<uint64_t> remote_hosts;
	std::set<uint64_t> need_delete;

	//1.确保dns service返回的结果有host信息
	assert(rsp.host_size() != 0);

	//2.插入新增的host信息到_host_map中
	for (int i = 0; i < rsp.host_size(); ++i)
	{
		const lars::HostInfo &h = rsp.host(i);

		//得到主机的ip+port key值
		uint64_t key = ((uint64_t)h.ip() << 32) + h.port();
		
		//将远程的主机加入到remote_host集合中
		remote_hosts.insert(key);
		
		if (_host_map.find(key) == _host_map.end())
		{
			//如果当前的_host_map找不到当下的key，说明是新增的
			host_info *hi = new host_info(h.ip(), h.port(), lb_config.init_succ_cnt);
			if (hi == NULL)
			{
				fprintf(stderr, "new host_info error\n");
				exit(1);
			}
			_host_map[key] = hi;

			//将新增的host信息加入到空闲列表中
			_idle_list.push_back(hi);
		} 

	}

	//3.遍历本地map和远程dns返回的主机集合，得到需要被删除的主机
	for (host_map_it it = _host_map.begin(); it != _host_map.end(); ++it)
	{
		if (remote_hosts.find(it->first) == remote_hosts.end())
		{
			//代表当前主机，没有在远程的返回的主机中存在，说明当前主机已经被修改或删除
			need_delete.insert(it->first);
		}
	}
	
	//4 删除主机
	for (std::set<uint64_t>::iterator it = need_delete.begin(); it != need_delete.end(); ++it)
	{
		uint64_t key = *it;

		host_info *hi = _host_map[key];

		if (hi->overload == true)
		{
			//从过载列表中删除
			_overload_list.remove(hi);
		}
		else 
		{
			//从空闲列表删除
			_idle_list.remove(hi);
		}

		delete hi;
	}

	//更新最后的update时间
	last_update_time = current_time;
	//重置为NEW状态
	status = NEW;

	printf("update hosts from DNS Service modid %d, cmdid %d\n", _modid, _cmdid);
}

//从idle_list或者overload_list中得到一个host节点信息
void get_host_from_list(lars::GetHostResponse &rsp, host_list& l)
{
	//从list中选择第一个节点
	host_info* host = l.front();

	//将取出的host节点信息添加给rsp的hostInfo字段
	lars::HostInfo *hip = rsp.mutable_host();
	hip->set_ip(host->ip);
	hip->set_port(host->port);

	l.pop_front();
	l.push_back(host);
}


//获取一个可用host信息
int load_balance::choice_one_host(lars::GetHostResponse &rsp)
{
	//1.判断_idle_list是否为空，如果为空，表示目前没有空闲节点
	if (_idle_list.empty() == true)
	{
		//1.1 判断是否超过阈值probe_num
		//   如果超过，尝试从overload_list获取节点
		//   如果没有超过，返回给API层，此时全部主机都是overload状态
		if (_access_cnt >= lb_config.probe_num)
		{
			_access_cnt = 0;
			//从overload_list选择一个过载的节点
			get_host_from_list(rsp, _overload_list);
		}
		else
		{
			//明确的返回API层 已经过载了
			++_access_cnt;
			return lars::RET_OVERLOAD;
		}
	}
	else 
	{

		if (_overload_list.empty() == true)
		{
			//_idle_list里面是有host主机
			//选择一个idle节点返回即可
			get_host_from_list(rsp, _idle_list);
		}
		else
		{
			//overload_list不为空
			//尝试去从overload_list找节点
			if (_access_cnt >= lb_config.probe_num)
			{
				_access_cnt = 0;
				//从overload_list选择一个过载的节点
				get_host_from_list(rsp, _overload_list);
			}
			else
			{
				++_access_cnt;
				//没有触发尝试overload获取的条件
				//选择一个idle节点返回即可
				get_host_from_list(rsp, _idle_list);
			}
		}
	}
	return lars::RET_SUCC;
}


//获取host主机集合
void load_balance::get_all_hosts(std::vector<host_info*> &vec)
{
	for (host_map_it it = _host_map.begin(); it != _host_map.end(); ++it)
	{
		host_info *hi = it->second;
		vec.push_back(hi);
	}
}


//通过对当前主机的上报的结果调整内部节点idle和overload关系
void load_balance::report(int ip, int port, int retcode)
{
	long current_time = time(NULL);

	uint64_t key = ((uint64_t)ip << 32) + port;
	if (_host_map.find(key) == _host_map.end())
	{	
		return;
	}

	//取出当前主机信息
	host_info *hi = _host_map[key];

	//1.计数统计
	if (retcode == lars::RET_SUCC)
	{
		++hi->vsucc;
		++hi->rsucc;

		//连续成功次数
		++hi->contin_succ;
		//连续失败次数归零
		hi->contin_err = 0;
	}
	else
	{
		++hi->verr;
		++hi->rerr;

		//连续失败次数
		++hi->contin_err;
		//连续成功次数归零
		hi->contin_succ = 0;
	}

	//2.检查节点的状态（检查idle节点是否满足overload条件，检查overload节点是否满足idle条件）
	if (hi->overload == false && retcode != lars::RET_SUCC)
	{
		bool overload = false;

		//idle节点
		//计算失败率
		double err_rate = hi->verr * 1.0 / (hi->vsucc + hi->verr);

		if (err_rate > lb_config.err_rate)
		{
			//失败率超值
			overload = true;
		}
		//检查连续的失败次数
		if (overload == false && hi->contin_err >= (uint32_t)lb_config.contin_err_limit)
		{
			overload = true;
		}

		if (overload == true)
		{
			//已经判定为overload状态
			struct in_addr saddr;
			saddr.s_addr = htonl(hi->ip);
			printf("[%d, %d] host %s: %d change overload. succ %u, err %u\n", _modid, _cmdid, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

			//切换为overload状态
			hi->set_overload();

			//移出idle_list
			_idle_list.remove(hi);

			//加入overload_list中
			_overload_list.push_back(hi);
			return;
		}
	}
	else if (hi->overload == true && retcode == lars::RET_SUCC)
	{
		bool idle = false;

		//overload节点
		//1.计算成功率
		double succ_rate = hi->vsucc * 1.0 / (hi->vsucc + hi->verr);

		if (succ_rate > lb_config.succ_rate)
		{
			idle = true;
		}

		//2.连续成功次数
		if (idle == false && hi->contin_succ >= (uint32_t)lb_config.contin_succ_limit)
		{
			idle = true;
		}

		if (idle == true)
		{
			//已经判定为idle状态
			struct in_addr saddr;
			saddr.s_addr = htonl(hi->ip);
			printf("[%d, %d] host %s: %d change overload. succ %u, err %u\n", _modid, _cmdid, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

			//切换为idle状态
			hi->set_idle();

			//移出overload_list
			_overload_list.remove(hi);

			//加入idle_list中
			_idle_list.push_back(hi);
			return;
		}
	}
	
	//idle的节点成了，overload的失败了 ---> 周期检查
	if (hi->overload == false)
	{
		//节点是一个idle节点
		if (current_time - hi->idle_ts >= lb_config.idle_timeout)
		{
			//当前的空闲节点，时间窗口到达
			//重置窗口，清空之前的累计成功次数
			struct in_addr saddr;
			saddr.s_addr = htonl(hi->ip);

			printf("[%d, %d] host %s:%d idle timeout! reset data to idle , vsucc %u, verr %u\n", _modid, _cmdid, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

			hi->set_idle();
		}
	}
	else
	{
		//当前节点是overload节点
		if (current_time- hi->overload_ts >= lb_config.overload_timeout)
		{
			//强制给overload节点拿到idle下，做尝试
			struct in_addr saddr;
			saddr.s_addr = htonl(hi->ip);

			printf("[%d, %d] host %s:%d overload timeout! reset idle , vsucc %u, verr %u\n", _modid, _cmdid, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

			hi->set_idle();
			_overload_list.remove(hi);
			_idle_list.push_back(hi);
		}
	}
}


//将最终的结果上报给report service
void load_balance::commit()
{
	if (this->empty())
	{
		return;
	}

	//1.封装消息
	lars::ReportStatusRequest req;
	req.set_modid(_modid);
	req.set_cmdid(_cmdid);
	req.set_ts(time(NULL));
	//默认当前的agent为caller
	req.set_caller(127);

	//2.从idle_list中取值，全部上报
	for (host_list_it it = _idle_list.begin(); it != _idle_list.end(); ++it)
	{
		host_info *hi = *it;
		lars::HostCallResult call_res;

		call_res.set_ip(hi->ip);
		call_res.set_port(hi->port);
		call_res.set_succ(hi->rsucc);
		call_res.set_err(hi->rerr);
		call_res.set_overload(false);

		req.add_results()->CopyFrom(call_res);
	}

	//3.从overload_list中取值，全部上报
	for (host_list_it it = _overload_list.begin(); it != _overload_list.end(); ++it)
	{
		host_info *hi = *it;
		lars::HostCallResult call_res;

		call_res.set_ip(hi->ip);
		call_res.set_port(hi->port);
		call_res.set_succ(hi->rsucc);
		call_res.set_err(hi->rerr);
		call_res.set_overload(true);

		req.add_results()->CopyFrom(call_res);
	}

	//将上报的请求数据发送report_client线程
	report_queue->send(req);
}
