#include <iostream>
#include <sys/types.h> 
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include "tcp_server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "reactor_buf.h"
#include "tcp_conn.h"
#include "config_file.h"

#if 0
void server_rd_callback(event_loop *loop, int fd, void *args);
void server_wt_callback(event_loop *loop, int fd, void *args);
#endif

//===========针对链接管理的初始化==============
tcp_conn **tcp_server::conns = NULL;
pthread_mutex_t tcp_server::_conns_mutex = PTHREAD_MUTEX_INITIALIZER;
int tcp_server::_curr_conns = 0;
int tcp_server::_max_conns = 0;

//===========初始化 路由分发机制句柄===========
msg_router tcp_server::router;

// =========== 初始化 Hook函数 =====
conn_callback tcp_server::conn_start_cb = NULL;
conn_callback tcp_server::conn_close_cb = NULL;
void * tcp_server::conn_start_cb_args = NULL;
void * tcp_server::conn_close_cb_args = NULL;

//新增一个链接
void tcp_server::increase_conn(int connfd, tcp_conn *conn)
{
	pthread_mutex_lock(&_conns_mutex);

	conns[connfd] = conn;
	++_curr_conns;
	
	pthread_mutex_unlock(&_conns_mutex);
}

//减少一个链接
void tcp_server::decrease_conn(int connfd)
{
	pthread_mutex_lock(&_conns_mutex);

	conns[connfd] = NULL;
	--_curr_conns;
	
	pthread_mutex_unlock(&_conns_mutex);
}

//得到当前的链接的个数
void tcp_server::get_conn_num(int *curr_conn)
{
	pthread_mutex_lock(&_conns_mutex);

	*curr_conn = _curr_conns;
	
	pthread_mutex_unlock(&_conns_mutex);
}
//============以上都是链接管理相关函数=================

void lars_hello()
{
	std::cout << "lars Hello" << std::endl;
}

#if 0
//临时的收发消息结构
struct message
{
	char data[m4K];
	char len;
};

struct message msg;
#endif

void accept_callback(event_loop *loop, int fd, void *args)
{
	tcp_server *server = (tcp_server *)args;
	server->do_accept();
}

#if 0
//客户端connfd 注册的写事件回调业务
void server_wt_callback(event_loop *loop, int fd, void *args)
{
	printf("start write\n");

	struct message *msg = (struct message *) args;

	output_buf obuf;

	//需要将msg存入obuf
	obuf.send_data(msg->data, msg->len);
	
	while (obuf.length())
	{
		int write_num = obuf.write2fd(fd);
		if (write_num == -1)
		{
			fprintf(stderr, "write connfd error\n");
			return;
		}
		else if (write_num == 0)
		{
			//当前不可写
			break;
		}
	}

	printf("write complete \n");
	//删除写事件，添加读事件
	loop->del_io_event(fd, EPOLLOUT);
	loop->add_io_event(fd, server_rd_callback, EPOLLIN, msg);

}

//客户端connfd 注册的读事件的回调业务
void server_rd_callback(event_loop *loop, int fd, void *args)
{
	struct message *msg = (struct message *)args;

	int ret = 0;
	
	input_buf ibuf;

	ret = ibuf.read_data(fd);
	if (ret == -1)
	{	
		fprintf(stderr, "ibuf read_data error\n");

		//当前的读事件删除
		loop->del_io_event(fd);

		//关闭对端fd
		close(fd);
		return;
	}
	else if (ret == 0)
	{
		//对方正常关闭
		//当前的读事件删除
		loop->del_io_event(fd);

		//关闭对端fd
		close(fd);
		return;
	}

	printf("server rd callback is called!\n");


	//将读到的数据拷贝到msg中
	msg->len = ibuf.length();
	bzero(msg->data, msg->len);
	memcpy(msg->data, ibuf.data(), msg->len);

	ibuf.pop(msg->len);
	ibuf.adjust();

	printf("recv data = %s\n", msg->data);

	//删除读事件，添加写事件
	loop->del_io_event(fd, EPOLLIN);
	loop->add_io_event(fd, server_wt_callback, EPOLLOUT, msg);//epoll_wait会直接触发写事件
}
#endif

//构造函数
tcp_server::tcp_server(event_loop *loop, const char *ip, uint16_t port)
{
	//0.忽略一些信号 SIGHUP, SIGPIPE
	if(signal(SIGHUP, SIG_IGN) == SIG_ERR)
	{
		fprintf(stderr, "signal ignore SIGHUP error\n");
	}

	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
		fprintf(stderr, "signal ignore SIGPIPE error\n");
	}

	//1.创建socket
	_sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
	if(_sockfd == -1)
	{
		fprintf(stderr, "tcp::server socket()\n");
		exit(1);
	}

	//2.初始化服务器的地址
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_aton(ip, &server_addr.sin_addr);
	server_addr.sin_port = htons(port);

	//2.5 设置socket可以重复使用
	int opt = 1;
	if(setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		fprintf(stderr, "set sockopt reuse error\n");
	}


	//3.绑定端口
	if(bind(_sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		fprintf(stderr, "bind error\n");
		exit(1);
	}

	//4.监听
	if(listen(_sockfd, 500) == -1)
	{
		fprintf(stderr, "listen error\n");
		exit(1);
	}

	//5.将形参loop添加到tcp_server _loop中
	_loop = loop;

	//6.创建线程池
	//从配置文件中读取（建议配置的个数和用来网络通信cpu数量一致）
	int thread_cnt  = config_file::instance()->GetNumber("reactor", "threadNum", 3);	
	if (thread_cnt > 0)
	{
		_thread_pool = new thread_pool(thread_cnt);
		if (_thread_pool == NULL)
		{
			fprintf(stderr, "tcp server new thread_pool error\n");
			exit(1);
		}
	}

	//7.创建链接管理
	_max_conns = config_file::instance()->GetNumber("reactor", "maxConn", 1000);//从配置文件去读
	conns = new tcp_conn*[_max_conns+5+2*thread_cnt];//3表示默认的3个系统文件描述符
	if(conns == NULL)
	{
		fprintf(stderr, "new conns %d error\n", _max_conns);
		exit(1);
	} 

	//8.注册_sockfd 读事件----> accept处理
	_loop->add_io_event(_sockfd, accept_callback, EPOLLIN, this);
}

//开始提供创建连接的服务
void tcp_server::do_accept()
{
	int connfd;
	while(1)
	{
		//1.accept
		_addrlen = 1;
		connfd = accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
		if(connfd == -1)
		{
			if (errno == EINTR)
			{
				//中断错误
				fprintf(stderr, "accept errno == EINTR\n");
				continue;
			}
			else if (errno == EAGAIN)
			{
				//非阻塞错误
				fprintf(stderr, "accept errno == EAGAIN\n");
				break;
			}
			else if (errno == EMFILE)
			{
				//建立链接过多，资源不够				
				fprintf(stderr, "accept errno == EMFILE\n");
				continue;
			}
			else
			{
				fprintf(stderr, "accept error\n");
				exit(1);
			}
		}
		else
		{
			//TODO 添加一些心跳机制

			//TODO 添加消息队列机制
			//accept成功
			printf("accpet succ!\n");

			//判断链接个数是否 已经超过最大值
			int cur_conns;
			get_conn_num(&cur_conns);
			if (cur_conns >= _max_conns)
			{
				fprintf(stderr, "so many connections, max = %d\n", _max_conns);
				close(connfd);
			}
			else
			{
				if (_thread_pool != NULL)
				{	
					//开启了多线程模式
					//将这个connfd交给一个线程来去创建并且去监听
					//1. 从线程池中获取一个thread_queue
					thread_queue<task_msg> *queue = _thread_pool->get_thread();

					//创建一个任务
					task_msg task;
					task.type = task_msg::NEW_CONN;
					task.connfd = connfd;

					//2. 将connfd发送到thread_queue中
					queue->send(task);
				}
				else 
				{
					//没有开启多线程模式
					//创建一个新的tcp_conn链接对象，将当前的链接用tcp_server来监听，是一个单线程的server
					tcp_conn *conn = new tcp_conn(connfd, _loop);
					if (conn == NULL)
					{
						fprintf(stderr, "new tcp_conn error");
						exit(1);
					}

					printf("get new connection succ!\n");
				}
				break;
			}
		}
	}
}

//析构函数 释放资源
tcp_server::~tcp_server()
{
	close(_sockfd);
}
