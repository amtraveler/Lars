#pragma once


/*
*
*抽象的链接类 
*
*/
class net_connection
{
public:
	net_connection(){}

	//纯虚函数
	virtual int send_message(const char *data, int msglen, int msgid) = 0;

	virtual int get_fd() = 0;

	void *param; //开发者可以通过该参数传递一些动态的自定义参数
};


//创建链接/销毁链接 要触发的回调函数类型
typedef void (*conn_callback)(net_connection *conn, void *args);

