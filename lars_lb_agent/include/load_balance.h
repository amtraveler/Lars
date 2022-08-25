#pragma once

#include <ext/hash_map>
#include "host_info.h"
#include <list>
#include "lars.pb.h"

//ip和port作为主键， host信息集合map
typedef	__gnu_cxx::hash_map<uint64_t, host_info*> host_map;//key-->uint64_t(ip+port), value-->host_info
typedef	__gnu_cxx::hash_map<uint64_t, host_info*>::iterator host_map_it;


typedef std::list<host_info*> host_list;
typedef std::list<host_info*>::iterator host_list_it;

/*
*负载均衡模块
*是针对一个（modid/cmdid）下的全部的host节点做负载规则
*/
class load_balance
{
public:
	
	load_balance(int modid, int cmdid)
	{
		_modid = modid;
		_cmdid = cmdid;
		_access_cnt = 0;
		status = NEW;
		last_update_time = time(NULL);
	}
	
	bool empty()
	{
		return _host_map.empty();
	}

	//获取一个可用host信息
	int choice_one_host(lars::GetHostResponse &rsp);

	//获取host主机集合
	void get_all_hosts(std::vector<host_info*> &vec);

	//向远程的DNS service中发送ID_GetRouteRequest请求
	int pull();

	//根据Dns service远程返回的主机结果，更新自己的host_map表
	void update(lars::GetRouteResponse &rsp);

	
	//通过对当前主机的上报的结果调整内部节点idle和overload关系
	void report(int ip, int port, int retcode);

	//将最终的结果上报给report service
	void commit();

	enum STATUS
	{
		PULLING,//正在从远程dns servuce网络通信中
		NEW //正在新建的load_balance模块
	};

	STATUS status; //当前状态

	//当前load_balance的最后更新时间
	long last_update_time;
private:
	int _modid;
	int _cmdid;

	//host_map 当前负载均衡模块所管理的全部主机
	host_map _host_map;

	//空闲队列
	host_list _idle_list;

	//过载队列
	host_list _overload_list;

	//当前modid/cmdid的请求次数
	int _access_cnt;

};
