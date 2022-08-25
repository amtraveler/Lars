#include "lars_reactor.h"
#include "lars.pb.h"


void report_host_status(net_connection *conn, void *args)
{
	//组装一个上报的消息请求
	lars::ReportStatusRequest req;

	req.set_modid(2);
	req.set_cmdid(2);
	req.set_caller(888);
	req.set_ts(time(NULL));

	//添加一些需要上报的主机消息
	for (int i = 0; i < 3; ++i)
	{
		lars::HostCallResult result;
		result.set_ip(i + 1);
		result.set_port(i + 100);

		result.set_succ(100);
		result.set_err(3);
		result.set_overload(true);

		req.add_results()->CopyFrom(result);
	}

	std::string requestString;
	req.SerializeToString(&requestString);

	//将消息发送给服务器
	conn->send_message(requestString.c_str(), requestString.size(), lars::ID_ReportStatusRequest);
}



int main()
{
	event_loop loop;

	tcp_client client(&loop, "127.0.0.1", 7779);

	//添加链接成功业务
	client.set_conn_start(report_host_status);

	loop.event_process();
}
