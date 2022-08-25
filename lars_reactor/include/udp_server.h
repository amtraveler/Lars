#pragma once

#include "event_loop.h"
#include <netinet/in.h>
#include "message.h"
#include "net_connection.h"
#include <signal.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

class udp_server : public net_connection
{
public:
	udp_server(event_loop *loop, const char *ip, uint16_t port);

	//udp主动发送消息
	virtual int send_message(const char *data, int msglen, int msgid);
	
	virtual int get_fd()
	{	
		return this->_sockfd;
	}
	//注册一个msgid和回调业务的路由关系
	void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL);

	~udp_server();

	//处理客户端数据的业务
	void do_read();
private:
	int _sockfd;

	event_loop *_loop;

	char _read_buf[MESSAGE_LENGTH_LIMIT];
	char _write_buf[MESSAGE_LENGTH_LIMIT];

	//客户端的ip地址
	struct sockaddr_in _client_addr;
	socklen_t _client_addrlen;

	//消息路由分发机制
	msg_router _router;
};
