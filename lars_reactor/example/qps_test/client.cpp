#include "tcp_client.h"
#include <stdio.h>
#include <string.h>
#include "echoMessage.pb.h"
#include <pthread.h>

struct Qps
{
	Qps()
	{
		last_time = time(NULL);
		succ_cnt = 0;
	}
	long last_time;//最后一次发包的时间 ms单位
	int succ_cnt; //成功收到服务器回显业务的次数
};

void qps_busi(const char *data, uint32_t len, int msgid, net_connection *conn, void *user_data)
{
	Qps *qps = (Qps*)user_data;
	qps_test::EchoMessage request, response;

	//解析server返回的数据包
	if (response.ParseFromArray(data, len) == false)
	{
		printf("server call back data error");
		return;
	}
	//判断消息的内容是否一致
	if (response.content() == "QPS test!")
	{
		//服务器回显任务成功，认为请求一次成功 qps++
		qps->succ_cnt++;
	}

	//得到当前的时间
	long current_time = time(NULL);
	if (current_time - qps->last_time >= 1)
	{
		//如果当前时间比记录时间大1秒，就需要统计qps成功的次数
		printf("QPS = %d\n", qps->succ_cnt);

		qps->last_time = current_time;//记录最后时间
		qps->succ_cnt = 0;//成功的次数
	}

	//发送request给客户端
	request.set_id(response.id() + 1);
	request.set_content(response.content());

	std::string requestString;
	request.SerializeToString(&requestString);

	conn->send_message(requestString.c_str(), requestString.size(), msgid);
}


//客户端创建链接之后Hook业务
void on_client_build(net_connection *conn, void *args)
{
	//当跟server端建立链接成功之后， 主动给server发送一个包
	qps_test::EchoMessage request;
	
	request.set_id(1);
	request.set_content("QPS test!");


	std::string requestString;
	request.SerializeToString(&requestString);

	//发送给server一个消息
	int msgid = 1;
	conn->send_message(requestString.c_str(), requestString.size(), msgid);
}

void *thread_main(void *args)
{
	event_loop loop;
	tcp_client client(&loop,"127.0.0.1", 7777);

	Qps qps;//创建一个qps记录的句柄

	//注册回调业务
	client.add_msg_router(1, qps_busi, (void*)&qps);

	//注册Hook函数
	client.set_conn_start(on_client_build);

	loop.event_process();

	return NULL;
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		printf("Usage: ./client [threadNum]\n");
	}

	int thread_num = atoi(argv[1]);

	pthread_t *tids;
	tids = new pthread_t[thread_num];

	for (int i = 0; i < thread_num; ++i)
	{
		pthread_create(&tids[i], NULL, thread_main, NULL);
	}

	for (int i = 0; i < thread_num; ++i)
	{
		pthread_join(tids[i], NULL);
	}
	
	return 0;
}
