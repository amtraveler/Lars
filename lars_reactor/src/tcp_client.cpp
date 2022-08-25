#include "tcp_client.h"
#include "message.h"
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


void read_callback(event_loop *loop, int fd, void *args)
{
	tcp_client *cli = (tcp_client *)args;
	cli->do_read();
}

void write_callback(event_loop *loop, int fd, void *args)
{
	tcp_client *cli = (tcp_client *)args;
	cli->do_write();
}

tcp_client::tcp_client(event_loop *loop, const char *ip, unsigned short port):_router()
{
	_sockfd = -1;
	//_msg_callback = NULL;
	_loop = loop;
	_conn_start_cb = NULL;
	_conn_close_cb = NULL;
	_conn_start_cb_args = NULL;
	_conn_close_cb_args = NULL;

	//封装即将要链接的远程server端的ip地址
	bzero(&_server_addr, sizeof(_server_addr));

	_server_addr.sin_family = AF_INET;
	inet_aton(ip, &_server_addr.sin_addr);
	_server_addr.sin_port = htons(port);
	_addrlen = sizeof(_server_addr);

	//链接远程客户端
	this->do_connect();
}

//发送方法
int tcp_client::send_message(const char *data, int msglen, int msgid)
{
	//printf("tcp_client::send_message()...\n");

	bool active_epollout = false;

	if (obuf.length() == 0)
	{
		//现在obuf是空的，之前的数据已经发送完毕，需要再发送 需要激活epoll的写事件回调
		//不为空需要等数据发送完才激活写事件
		active_epollout = true;
	}

	//1. 封装一个message报头
	msg_head head;
	head.msgid = msgid;
	head.msglen = msglen;

	int ret = obuf.send_data((const char *)&head, MESSAGE_HEAD_LEN);
	if (ret != 0)
	{
		fprintf(stderr, "send head error\n");
		return -1;
	}

	//2. 写消息体
	ret = obuf.send_data(data, msglen);
	if (ret != 0)
	{
		fprintf(stderr, "send data error\n");

		//如果消息体写失败，消息头需要弹出重置
		obuf.pop(MESSAGE_HEAD_LEN);
		return -1;
	}

	//3. 将_connfd添加一个写事件EPOLLOUT，让回调函数直接将obuf中的数据写给对方
	if (active_epollout == true)
	{
		//printf("send_message active epoll out!\n");
		_loop->add_io_event(_sockfd, write_callback, EPOLLOUT, this);
	}

	return 0;
}

//处理读业务
void tcp_client::do_read()
{
	//printf("tcp_client::do_read()...\n");

	//1.从connfd中去读数据
	int ret = ibuf.read_data(_sockfd);
	if (ret == -1)
	{
		fprintf(stderr, "read data from socket\n");
		this->clean_conn();
		return;
	}
	else if(ret == 0)
	{
		//对方正常关闭
		printf("peer client closed!\n");
		this->clean_conn();
	}
	
	//读出来的消息头
	msg_head head;

	//2.读过来的数据是否满足8字节
	while (ibuf.length() >= MESSAGE_HEAD_LEN)
	{		
		//2.1 先读头部，得到msgid， msglen
		memcpy(&head, ibuf.data(), MESSAGE_HEAD_LEN);
		if (head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0)
		{
			fprintf(stderr, "data format error, too large or too small, need close\n");
			this->clean_conn();
			break;
		}	

		//2.2 判断得到的消息体的长度和头部里的长度是否一致
		if (ibuf.length() < MESSAGE_HEAD_LEN + head.msglen)
		{
			//缓存中buf剩余的数据，小于应该接受到的数据，说明当前不是一个完整的包
			break;
		}
		
		//表示当前包是合法的
		ibuf.pop(MESSAGE_HEAD_LEN);


		//将得到的数据，做一个回显业务
#if 0
		if (_msg_callback != NULL)
		{
			this->_msg_callback(ibuf.data(), head.msglen, head.msgid, this, NULL);
		}
#endif
		//调用注册的回调业务
		this->_router.call(head.msgid, head.msglen, ibuf.data(), this);

		//整个消息都处理完了
		ibuf.pop(head.msglen);
	}

	ibuf.adjust();
}

//处理写业务
void tcp_client::do_write()
{
	//printf("tcp_client::do_write()...\n");

	while (obuf.length())
	{
		int write_num = obuf.write2fd(_sockfd);
		if (write_num == -1)
		{
			fprintf(stderr, "tcp_client write sockfd error\n");
			this->clean_conn();
			return;
		}
		else if (write_num == 0)
		{
			//当前不可写
			break;
		}
	}

	if (obuf.length() == 0)
	{
		//数据已经全部写完了，将_connfd的写事件删掉
		_loop->del_io_event(_sockfd, EPOLLOUT);
	}

}

//释放链接
void tcp_client::clean_conn()
{
	//调用链接销毁之前的Hook函数
	if (_conn_close_cb != NULL)
	{
		_conn_close_cb(this, _conn_close_cb_args);
	}

	if (_sockfd != -1)
	{
		printf("clean conn, del socket\n");
		_loop->del_io_event(_sockfd);
		close(_sockfd);
	}

	//重新发起连接
	//this->do_connect();
}

//如果该函数被执行，则链接成功
void connection_succ(event_loop *loop, int fd, void *args)
{
	tcp_client *cli = (tcp_client *)args;
	loop->del_io_event(fd);

	//对当前fd进行一次错误编码的获取，如果没有任何错误，那么一定是成功了
	//如果有的话，认为fd是没有创建成功，连接失败
	int result = 0;
	socklen_t result_len = sizeof(result);
	getsockopt(fd, SOL_SOCKET, SO_ERROR, &result, &result_len);
	if (result == 0)
	{
		//fd中没有任何错误
		//链接创建成功
		//printf("connection succ!\n");

		//链接创建成功后的业务

		//客户端业务
		if (cli->_conn_start_cb != NULL)
		{
			cli->_conn_start_cb(cli, cli->_conn_start_cb_args);
		}


		//添加针对当前cli fd的读回调
		loop->add_io_event(fd, read_callback, EPOLLIN, cli);

		if (cli->obuf.length() != 0)
		{
			//让event_loop触发写回调业务
			loop->add_io_event(fd, write_callback, EPOLLOUT, cli);
		}
	}
	else
	{
		fprintf(stderr, "connection %s:%d error\n", inet_ntoa(cli->_server_addr.sin_addr), ntohs(cli->_server_addr.sin_port));
		return ;
	}

	
}

//连接服务器
void tcp_client::do_connect()
{
	//connect...
	if (_sockfd != -1)
	{
		close(_sockfd);
	}

	//创建套接字(非阻塞)
	_sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, IPPROTO_TCP);
	if (_sockfd == -1)
	{
		fprintf(stderr, "create tcp client socket error\n");
		exit(1);
	}

	int ret = connect(_sockfd, (const struct sockaddr *)&_server_addr, _addrlen);
	if (ret == 0)
	{
		//创建成功
		//printf("connect ret =0, connect %s:%d succ!\n", inet_ntoa(_server_addr.sin_addr), ntohs(_server_addr.sin_port));

		connection_succ(_loop, _sockfd, this);
	}
	else
	{
		//创建失败
		if (errno == EINPROGRESS)
		{
			//fd是非阻塞的，会出现这个错误，但并不是创建链接错误
			//如果fd是可写的，那么链接创建时成功的
			printf("do_connect, EINPROGRESS\n");

			//让event_loop去检测当前的sockfd是否可写，如果当前回调被执行，说明可写
			_loop->add_io_event(_sockfd, connection_succ, EPOLLOUT, this);
		}
		else
		{
			fprintf(stderr, "connection error\n");
			exit(1);
		}
	}

}
