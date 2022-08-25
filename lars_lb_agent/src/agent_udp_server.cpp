#include "main_server.h"

void report_cb(const char* data, uint32_t len, int msgid, net_connection* conn, void *user_data)
{
	lars::ReportRequest req;

	req.ParseFromArray(data, len);

	route_lb* route_lb_p = (route_lb*)user_data;
	route_lb_p->report_host(req);

	//printf("udp server call report_cb\n");
}


void get_host_cb(const char* data, uint32_t len, int msgid, net_connection* conn, void* user_data)
{
	lars::GetHostRequest req;
	req.ParseFromArray(data, len);

	int modid = req.modid();
	int cmdid = req.cmdid();

	//设置回复消息
	lars::GetHostResponse rsp;
	rsp.set_seq(req.seq());
	rsp.set_modid(modid);
	rsp.set_cmdid(cmdid);

	//通过route_lb获取一个可用host添加到rsp中
	route_lb *route_lb_p = (route_lb*)user_data;
	route_lb_p->get_host(modid, cmdid, rsp);

	//将rsp发送回给api
	std::string responseString;
	rsp.SerializeToString(&responseString);

	conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetHostResponse);
}


void get_route_cb(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	lars::GetRouteRequest req;
	req.ParseFromArray(data, len);

	int modid = req.modid();
	int cmdid = req.cmdid();

	//设置回复消息
	lars::GetRouteResponse rsp;
	rsp.set_modid(modid);
	rsp.set_cmdid(cmdid);

	//通过route_lb获取一个可用host添加到rsp中
	route_lb *route_lb_p = (route_lb*)user_data;
	route_lb_p->get_route(modid, cmdid, rsp);
	
	//将rsp发送回给api
	std::string responseString;
	rsp.SerializeToString(&responseString);

	conn->send_message(responseString.c_str(), responseString.size(), lars::ID_API_GetRouteResponse);
}


//一个udp server
void *agent_server_main(void *args)
{
	long index = (long)args;

	short port = index + 8888;

	event_loop loop;

	udp_server server(&loop, "127.0.0.1", port);

	//给udp server注册一些消息路由业务

	//针对API的获取主机信息接口
	server.add_msg_router(lars::ID_GetHostRequest, get_host_cb, r_lb[port-8888]);

	//针对API的上报主机信息接口
	server.add_msg_router(lars::ID_ReportRequest, report_cb, r_lb[port-8888]);

	//针对API获取路由全部主机信息的接口
	server.add_msg_router(lars::ID_API_GetRouteRequest, get_route_cb, r_lb[port-8888]);

	printf("agent UDP server : port %d is start...\n", port);

	loop.event_process();

	return NULL;
}


void start_UDP_server(void)
{
	for (long i = 0; i < 3; ++i)
	{
		pthread_t tid;
		
		int ret = pthread_create(&tid, NULL, agent_server_main, (void*)i);
		if (ret == -1)
		{
			perror("pthread create udp error\n");
			exit(1);
		}

		pthread_detach(tid);
	}
}
