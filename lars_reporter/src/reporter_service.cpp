#include "lars_reactor.h"
#include "lars.pb.h"
#include "store_report.h"


tcp_server *server;

thread_queue<lars::ReportStatusRequest>** reportQueues = NULL;

int thread_cnt;

void get_report_status(const char *data, uint32_t len, int msgid, net_connection*conn, void *user_data)
{
	lars::ReportStatusRequest req;
	req.ParseFromArray(data, len);


	//将当前的请求入库的 交给其中一个线程来处理
	static int index = 0;
	reportQueues[index]->send(req);
	++index;
	index = index % thread_cnt;
}

void create_report_threads(void)
{
	thread_cnt = config_file::instance()->GetNumber("reporter", "db_thread_cnt", 3);


	//线程的消息队列
	reportQueues = new thread_queue<lars::ReportStatusRequest>*[thread_cnt];
	if (reportQueues == NULL)
	{
		fprintf(stderr,  "create thread queue  error\n");
		exit(1);
	}

	for (int i = 0; i < thread_cnt; ++i)
	{
		//给每个线程的消息队列开辟空间
		reportQueues[i] = new thread_queue<lars::ReportStatusRequest>;
		if (reportQueues[i] == NULL)
		{
			fprintf(stderr, "create thread queue %d error\n", i);
			exit(1);
		}

		pthread_t tid;
		int ret = pthread_create(&tid, NULL, store_main, reportQueues[i]);
		if (ret == -1)
		{
			perror("pthread create");
			exit(1);
		}

		pthread_detach(tid);
	}
}


int main()
{
	event_loop loop;

	//加载配置文件
	config_file::setPath("./conf/lars_reporter.ini");
	std::string ip = config_file::instance()->GetString("reactor", "ip", "0.0.0.0");
	short port = config_file::instance()->GetNumber("reactor", "port", 8888);


	server = new tcp_server(&loop, ip.c_str(), port);
	
	//添加上报请求的处理 消息分发业务
	server->add_msg_router(lars::ID_ReportStatusRequest, get_report_status);
	
	//防止业务出现磁盘io阻塞延迟，导致网络请求不能及时响应
	//启动一个存储的线程池，针对磁盘io进行入库操作
	create_report_threads();

	loop.event_process();

	return 0;
}
