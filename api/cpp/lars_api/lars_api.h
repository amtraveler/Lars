#pragma once
#include <string>
#include "lars_reactor.h"
#include <vector>

typedef std::pair<std::string, int> ip_port;
typedef std::vector<ip_port> route_set;
typedef route_set::iterator route_set_it;

class lars_client
{
public:
	lars_client();
	~lars_client();

	//注册一个模块
	int reg_init(int modid, int cmdid);

	//lars系统获取host信息，得到可用的host ip和port
	int get_host(int modid, int cmdid, std::string &ip, int &port);

	//lars获取某modid/cmdid的全部hosts
	int get_route(int modid, int cmdid, route_set &route);

	//lars系统上报host调用信息api
	void report(int modid, int cmdid, std::string &ip, int port, int retcode);	

private:
	int _sockfd[3]; //对应3个lars系统udpserver
	uint32_t _seqid; //每个消息的序号，为了判断返回udp包是否合法
};
