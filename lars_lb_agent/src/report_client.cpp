#include "main_server.h"

//如果队列中有数据，所触发的一个回调业务
void new_report_request(event_loop* loop, int fd, void *args)
{
	tcp_client *client = (tcp_client*)args;

	//1 将请求数据从dns_queue中取出
	std::queue<lars::ReportStatusRequest> msgs;

	//2 将取出的数据放在一个queue容器中
	report_queue->recv(msgs);

	//3 遍历queue容器元素，依次将每个元素消息发送给dns service
	while (!msgs.empty())
	{
		lars::ReportStatusRequest req = msgs.front();
		msgs.pop();

		std::string requestString;
		req.SerializeToString(&requestString);

		//将这个消息透传给dns service
		client->send_message(requestString.c_str(), requestString.size(), lars::ID_ReportStatusRequest);
	}
}

void* report_client_thread(void* args)
{
	printf("report client thread start!\n");

	event_loop loop;

	std::string ip = config_file::instance()->GetString("reporter", "ip", "127.0.0.1");
	short port = config_file::instance()->GetNumber("reporter", "port", 7779);

	//创建客户端
	tcp_client client(&loop, ip.c_str(), port);

	//将report_queue绑定到loop中，让loop监控queue的数据
	report_queue->set_loop(&loop);
	report_queue->set_callback(new_report_request, &client);

	loop.event_process();
	return NULL;
}


void start_report_client(void)
{
	//开辟一个线程
	pthread_t tid;

	int ret = pthread_create(&tid, NULL, report_client_thread, NULL);
	if (ret == -1)
	{
		perror("pthread_create");
		exit(1);
	}

	pthread_detach(tid);
}
