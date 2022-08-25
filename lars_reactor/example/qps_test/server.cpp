#include "tcp_server.h"
#include <string.h>
#include "config_file.h"
#include "echoMessage.pb.h"

//业务1 回显
void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	qps_test::EchoMessage request, response;

	//解包，把将data中的proto协议解析出来 放到request对象中
	request.ParseFromArray(data, len);


	//printf("content = %s\n", request.content());

	//回显业务 制作一个新的proto数据包 EchoMessage类型的包

	//赋值
	response.set_id(request.id());
	response.set_content(request.content());

	//将response序列化
	std::string responseString;
	response.SerializeToString(&responseString);

	//将回执的message发送给客户端
	conn->send_message(responseString.c_str(), responseString.size(), msgid);
}


int main()
{
	event_loop loop;

	//加载配置文件
	config_file::setPath("./reactor.ini");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
	short port = config_file::instance()->GetNumber("reactor", "port", 8888);


	tcp_server server(&loop, ip.c_str(), port);

	//注册回调方法
	server.add_msg_router(1, callback_busi);

	loop.event_process();

	return 0;
}
