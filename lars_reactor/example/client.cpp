#include "tcp_client.h"
#include <stdio.h>
#include <string.h>

void callback_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	//将数据写回去
	conn->send_message(data, len, msgid);
}

void print_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	//将数据打印
	printf("print busi is called!\n");
	printf("recv from server: %s\n", data);
	printf("msgid = %d\n", msgid);
	printf("len = %d\n", len);
}

//客户端创建链接之后Hook业务
void on_client_build(net_connection *conn, void *args)
{
	printf("===> on_client build!\n");

	//客户端一旦链接成功 就会主动给server发送一个msgid=1的消息
	int msgid = 1;
	const char *msg = "hello Lars\n";

	conn->send_message(msg, strlen(msg), msgid);
}

//客户端销毁链接之前的Hook函数
void on_client_lost(net_connection *conn, void *args)
{
	printf("===> on_client lost!\n");
}

int main()
{
	event_loop loop;
	tcp_client client(&loop,"127.0.0.1", 7777);

	//注册回调业务
	client.add_msg_router(1, print_busi);
	client.add_msg_router(2, callback_busi);
	client.add_msg_router(200, print_busi);
	client.add_msg_router(404, print_busi);

	//注册Hook函数
	client.set_conn_start(on_client_build);
	client.set_conn_close(on_client_lost);

	loop.event_process();
	return 0;
}
