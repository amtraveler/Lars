#include "udp_server.h"
#include <string.h>
#include "config_file.h"

//业务1 回显
void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	printf("callback busi is called!!!\n");

	//直接将数据发回去
	//此时net_connection *conn--> tcp_conn对象, conn就是能够跟对端通信得到tcp_conn链接对象
	conn->send_message(data, len, msgid);
	
}

int main()
{
	event_loop loop;

	//加载配置文件
	config_file::setPath("./reactor.ini");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
	short port = config_file::instance()->GetNumber("reactor", "port", 8888);


	udp_server server(&loop, ip.c_str(), port);

	//注册回调方法
	server.add_msg_router(1, callback_busi);

	loop.event_process();

	return 0;
}
