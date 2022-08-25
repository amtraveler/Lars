#pragma once

#include "net_connection.h"
#include "event_loop.h"
#include <netinet/in.h>
#include "message.h"
#include <signal.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


class udp_client : public net_connection
{
public:
	udp_client(event_loop *loop, const char *ip, uint16_t port);
	
	//udp主动发送消息
	virtual int send_message(const char *data, int msglen, int msgid);


	virtual int get_fd()
	{
		return this->_sockfd;
	}

	//注册一个msgid和回调业务的路由关系
	void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL);

	void do_read();

	~udp_client();

private:
	int _sockfd;

	event_loop *_loop;

	char _read_buf[MESSAGE_LENGTH_LIMIT];
	char _write_buf[MESSAGE_LENGTH_LIMIT];

	//消息路由分发机制
	msg_router _router;
};
