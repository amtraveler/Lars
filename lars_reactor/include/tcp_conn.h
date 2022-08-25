#pragma once

#include "event_loop.h"
#include "reactor_buf.h"
#include "net_connection.h"

class tcp_conn : public net_connection
{
public:
	//初始化conn
	tcp_conn(int connfd, event_loop *loop);

	//被动处理读业务的方法 由event_loop监听触发的
	void do_read();

	//被动处理写业务的方法 由event_loop监听触发的
	void do_write();

	//主动发送消息的方法
	virtual int send_message(const char *data, int msglen, int msgid);

	virtual int get_fd()
	{
		return this->_connfd;
	}

	//销毁当前链接
	void clean_conn();

private:
	//当前的链接conn的套接字fd
	int _connfd;

	//当前链接是归属于哪个event_loop的
	event_loop *_loop;

	//输出output_buf
	output_buf obuf;

	//输入input_buf
	input_buf ibuf;

};
