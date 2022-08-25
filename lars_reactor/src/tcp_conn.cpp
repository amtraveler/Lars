#include "tcp_conn.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "message.h"
#include "tcp_server.h"
#include  <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//当前连接的处理客户的业务 直接将数据回显给对方
void callback_busi(const char *data, uint32_t len, int msgid, void *args, tcp_conn *conn)
{
	//让tcp_conn主动发送消息给对应的client
	conn->send_message(data, len, msgid);
}

void conn_rd_callback(event_loop *loop, int fd, void *args)
{
	tcp_conn *conn = (tcp_conn*)args;
	conn->do_read();
}

void conn_wt_callback(event_loop *loop, int fd, void *args)
{
	tcp_conn *conn = (tcp_conn*)args;
	conn->do_write();
}

//初始化conn
tcp_conn::tcp_conn(int connfd, event_loop *loop)
{
	_connfd = connfd;
	_loop = loop;

	//1 将connfd设置成非阻塞状态
	int flag = fcntl(_connfd, F_SETFL, 0); //将connfd状态清空
	fcntl(_connfd, F_SETFL, O_NONBLOCK | flag); //设置为非阻塞

	//2 设置TCP_NODELAY状态，禁止读写缓存，降低小包延迟
	int op = 1;
	setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));

	//3 执行创建链接成功之后要触发的Hook函数
	if (tcp_server::conn_start_cb != NULL)
	{
		tcp_server::conn_start_cb(this, tcp_server::conn_start_cb_args);
	}

	//将当前tcp_conn的读事件 加入到loop中进行监听
	_loop->add_io_event(_connfd, conn_rd_callback, EPOLLIN, this);

	//将自己添加到 tcop_server中的conns集合中
	tcp_server::increase_conn(_connfd, this);
}

//被动处理读业务的方法 由event_loop监听触发的
void tcp_conn::do_read()
{
	//1.从connfd中去读数据
	int ret = ibuf.read_data(_connfd);
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

		//处理ibuf.data()业务数据
		//printf("read data = %s\n", ibuf.data());
		
		//针对不同的msgID 来调用不同的业务
		tcp_server::router.call(head.msgid, head.msglen, ibuf.data(), this);



		//整个消息都处理完了
		ibuf.pop(head.msglen);
	}

	ibuf.adjust();
}

//被动处理写业务的方法 由event_loop监听触发的
void tcp_conn::do_write()
{
	//do_write就表示obuf中已经有要写的数据，将obuf的数据发送给客户端	

	while (obuf.length())
	{
		int write_num = obuf.write2fd(_connfd);
		if (write_num == -1)
		{
			fprintf(stderr, "tcp_conn write connfd error\n");
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
		_loop->del_io_event(_connfd, EPOLLOUT);
	}
}

//主动发送消息的方法
int tcp_conn::send_message(const char *data, int msglen, int msgid)
{
	//printf("server send message: data:%s, msglen:%d, msgid:%d\n", data, msglen, msgid);
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
		_loop->add_io_event(_connfd, conn_wt_callback, EPOLLOUT, this);
	}
	return 0;
}

//销毁当前链接
void tcp_conn::clean_conn()
{
	//调用链接销毁之前要触发的Hook函数
	if (tcp_server::conn_close_cb != NULL)
	{
		tcp_server::conn_close_cb(this, tcp_server::conn_close_cb_args);
	}

	//将tcp_server中把当前链接摘除
	tcp_server::decrease_conn(_connfd);

	//链接的清理工作
	_loop->del_io_event(_connfd);

	ibuf.clear();
	obuf.clear();

	close(_connfd);
}

