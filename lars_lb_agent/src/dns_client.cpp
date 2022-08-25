#include "main_server.h"

void deal_recv_route(const char *data, uint32_t len, int msgid, net_connection* conn, void *user_data)
{
	lars::GetRouteResponse rsp;
	rsp.ParseFromArray(data, len);

	int modid = rsp.modid();
	int cmdid = rsp.cmdid();

	int index = (modid + cmdid) % 3;

	//将modid/cmdid交给一个route_lb来处理，将rsp中的hostinfo集合加入到对应route_lb中
	r_lb[index]->update_host(modid, cmdid, rsp);
}

//如果队列中有数据，所触发的一个回调业务
void new_dns_request(event_loop* loop, int fd, void *args)
{
	tcp_client *client = (tcp_client*)args;

	//1 将请求数据从dns_queue中取出
	std::queue<lars::GetRouteRequest> msgs;

	//2 将取出的数据放在一个queue容器中
	dns_queue->recv(msgs);

	//3 遍历queue容器元素，依次将每个元素消息发送给dns service
	while (!msgs.empty())
	{
		lars::GetRouteRequest req = msgs.front();
		msgs.pop();

		std::string requestString;
		req.SerializeToString(&requestString);

		//将这个消息透传给dns service
		client->send_message(requestString.c_str(), requestString.size(), lars::ID_GetRouteRequest);
	}
}

void conn_init(net_connection* conn, void *args)
{
	//dns client已经和dns service链接成功
	//让所有udp server的route_lb下所有loadbalance都重置为NEW状态
	for (int i = 0; i < 3; ++i)
	{
		r_lb[i]->reset_lb_status();
	}
}


//dns client线程的主业务
void* dns_client_thread(void* args)
{
	printf("dns client thread start!\n");
	
	event_loop loop;

	std::string ip = config_file::instance()->GetString("dns", "ip", "127.0.0.1");
	short port = config_file::instance()->GetNumber("dns", "port", 7778);

	//创建客户端
	tcp_client client(&loop, ip.c_str(), port);

	//将dns_queue绑定到loop中，让loop监控queue的数据
	dns_queue->set_loop(&loop);
	dns_queue->set_callback(new_dns_request, &client);

	//注册一个回调函数 用来处理dns server的返回的消息
	client.add_msg_router(lars::ID_GetRouteResponse, deal_recv_route);

	//设置一个当前dns client创建连接成功的Hook函数
	client.set_conn_start(conn_init);


	loop.event_process();


	return NULL;
}

void start_dns_client(void)
{
	//开辟一个线程
	pthread_t tid;

	int ret = pthread_create(&tid, NULL, dns_client_thread, NULL);
	if (ret == -1)
	{
		perror("pthread_create");
		exit(1);
	}

	pthread_detach(tid);

}
