#include "lars_reactor.h"
#include "dns_route.h"
#include "lars.pb.h"
#include "subscribe.h"


tcp_server *server;

typedef hash_set<uint64_t> client_sub_mod_list; //Agent客户端已经订阅的mod模块集合

//处理agent发送Route信息获取的业务
void get_route(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	//1 解析proto文件
	lars::GetRouteRequest req;
	req.ParseFromArray(data, len);

	//2 得到modID和cmdID
	int modid, cmdid;
	modid = req.modid();
	cmdid = req.cmdid();

	//2.5 如果之前没有订阅过modid/cmdid，则订阅
	uint64_t mod = ((uint64_t)modid << 32) + cmdid;
	client_sub_mod_list * sub_list = (client_sub_mod_list*)conn->param;
	if (sub_list == NULL)
	{
		fprintf(stderr, "sub_list = NULL \n");
	}
	else if (sub_list->find(mod) == sub_list->end())
	{
		//说明当前mod是没有被订阅的（首次请求当前mod），需要订阅
		sub_list->insert(mod);

		//订阅当前mod
		SubscribeList::instance()->subscribe(mod, conn->get_fd());
	}

	//3 通过modid/cmdid 获取host信息 从_data_pointer所指向map中
	host_set hosts = Route::instance()->get_hosts(modid, cmdid);

	//4 打包一个新的response protobuf数据
	lars::GetRouteResponse rsp;
	rsp.set_modid(modid);
	rsp.set_cmdid(cmdid);

	for (host_set_it it = hosts.begin(); it != hosts.end(); ++it)
	{
		//it就是set中的一个元素 ip+port 64位一个整形键值对
		uint64_t ip_port = *it;
		lars::HostInfo host;
		host.set_ip((uint32_t)(ip_port>>32));
		host.set_port((uint32_t)ip_port);

		//将host添加到rsp对象中
		rsp.add_host()->CopyFrom(host);
	}

	//5 发送给对方
	std::string responseString;
	rsp.SerializeToString(&responseString);
	conn->send_message(responseString.c_str(), responseString.size(), lars::ID_GetRouteResponse);
}

//每个新客户端创建成功之后，执行该函数
void create_subscribe(net_connection* conn, void *args)
{
	//给当前的conn绑定一个订阅的mod的一个set集合
	conn->param = new client_sub_mod_list;
}

//每个连接销毁之前需要调用
void clear_subscribe(net_connection* conn, void *args)
{
	client_sub_mod_list::iterator it;
	client_sub_mod_list * sub_list = (client_sub_mod_list*)conn->param;
	
	for (it = sub_list->begin(); it != sub_list->end(); ++it)
	{
		//取消dns的订阅
		uint64_t  mod = *it;
		
		SubscribeList::instance()->unsubscribe(mod, conn->get_fd());
	}
	delete sub_list;

	conn->param = NULL;
}

int main()
{
	event_loop loop;

	//加载配置文件
	config_file::setPath("./conf/lars_dns.ini");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
	short port = config_file::instance()->GetNumber("reactor", "port", 8888);


	server = new tcp_server(&loop, ip.c_str(), port);
	
	//注册创建/销毁链接的Hook函数
	server->set_conn_start(create_subscribe); 
	server->set_conn_close(clear_subscribe);

	//注册一个回调业务
	server->add_msg_router(lars::ID_GetRouteRequest, get_route);

	//开辟一个线程，定期的发布已经更变的mod集合
	pthread_t tid;
	//int ret = pthread_create(&tid, NULL, publish_change_mod_test, NULL);

	//启动backend thread实时监控版本信息
	int ret = pthread_create(&tid, NULL, check_route_changes, NULL);
	if (ret == -1)
	{
		perror("pthread_create error\n");
		exit(1);
	}	
	
	pthread_detach(tid);

	
	loop.event_process();

	return 0;
}
