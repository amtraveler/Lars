#include "lars_api.h"
//#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <strings.h>
#include <unistd.h>
#include "lars.pb.h"

using namespace std;

lars_client::lars_client()
{
	_seqid = 0;

	//1.初始化服务器地址
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	//默认的ip地址是本地，api和agent应该部署在同一台主机上
	inet_aton("127.0.0.1", &servaddr.sin_addr);

	//2.创建3个udp server
	for (int i = 0; i < 3; ++i)
	{
		_sockfd[i] = socket(AF_INET, SOCK_DGRAM|SOCK_CLOEXEC, IPPROTO_UDP);
		if (_sockfd[i] == -1)
		{
			perror("socket()");
			exit(1);
		}

		//本链接对应的udp server的端口号
		servaddr.sin_port = htons(8888 + i);

		//链接
		int ret = connect(_sockfd[i], (const struct sockaddr*)&servaddr, sizeof(servaddr));
		if (ret == -1)
		{
			perror("connect()\n");
			exit(1);
		}

		//cout << "connection agent udp server " << 8888 + i << " succ!" <<endl;
	}	

}

lars_client::~lars_client()
{
	//关闭3个upd链接
	for (int i = 0; i < 3; ++i)
	{
		close(_sockfd[i]);
	}
}

//lars系统获取host信息，得到可用的host ip和port
int lars_client::get_host(int modid, int cmdid, std::string &ip, int &port)
{
	uint32_t seq = _seqid++;
	//1.封装请求的protobuf消息
	lars::GetHostRequest req;
	req.set_seq(seq);
	req.set_modid(modid);
	req.set_cmdid(cmdid);

	//2.打包成lars能够识别的message
	char write_buf[4096], read_buf[20 * 4096];
	//消息头
	msg_head head;
	head.msglen = req.ByteSizeLong();
	head.msgid = lars::ID_GetHostRequest;

	memcpy(write_buf, &head, MESSAGE_HEAD_LEN);

	req.SerializeToArray(write_buf + MESSAGE_HEAD_LEN, head.msglen);

	//3.发送
	int index = (modid + cmdid) % 3;
	int ret = sendto(_sockfd[index], write_buf, head.msglen + MESSAGE_HEAD_LEN, 0, NULL, 0);
	if (ret ==-1)
	{
		perror("send to");
		return lars::RET_SYSTEM_ERROR;
	}

	//4.阻塞等待接收数据
	int message_len;
	
	lars::GetHostResponse rsp;
	
	do
	{
		message_len = recvfrom(_sockfd[index], read_buf, sizeof(read_buf), 0, NULL, 0);
		if (message_len == -1)
		{
			perror("recvfrom");
			return lars::RET_SYSTEM_ERROR;
		}

		//消息头
		memcpy(&head, read_buf, MESSAGE_HEAD_LEN);
		if (head.msgid != lars::ID_GetHostResponse)
		{
			fprintf(stderr, "message ID error\n");
			return lars::RET_SYSTEM_ERROR;
		}

		//消息体
		ret = rsp.ParseFromArray(read_buf + MESSAGE_HEAD_LEN, message_len - MESSAGE_HEAD_LEN);
		if (!ret)
		{
			fprintf(stderr, "message format error\n");
			return lars::RET_SYSTEM_ERROR;
		}
	} while(rsp.seq() < seq); //直到服务器返回一个跟当前发送的seq相同为止
	
	if (rsp.seq() != seq || rsp.modid() != modid || rsp.cmdid() != cmdid)
	{
		fprintf(stderr, "message format error\n");
		return lars::RET_SYSTEM_ERROR;
	}

	//5.处理消息
	if (rsp.retcode() == lars::RET_SUCC)
	{
		lars::HostInfo host = rsp.host();

		struct in_addr inaddr;
		inaddr.s_addr = host.ip();
		ip = inet_ntoa(inaddr);
		port = host.port();
	}

	return rsp.retcode();
}

//lars系统上报host调用信息api
void lars_client::report(int modid, int cmdid, std::string &ip, int port, int retcode)	
{
	//1.封装信息
	lars::ReportRequest req;

	req.set_modid(modid);
	req.set_cmdid(cmdid);
	req.set_retcode(retcode);

	lars::HostInfo *hp = req.mutable_host();

	struct in_addr inaddr;
	inet_aton(ip.c_str(), &inaddr);
	int ip_num = inaddr.s_addr;
	hp->set_ip(ip_num);
	hp->set_port(port);

	//2.send
	char write_buf[4096];
	
	//消息头
	msg_head head;
	head.msglen = req.ByteSizeLong();
	head.msgid = lars::ID_ReportRequest;
	memcpy(write_buf, &head, MESSAGE_HEAD_LEN);

	//消息体
	req.SerializeToArray(write_buf + MESSAGE_HEAD_LEN, head.msglen);

	int index = (modid + cmdid) % 3;
	int ret = sendto(_sockfd[index], write_buf, MESSAGE_HEAD_LEN + head.msglen, 0, NULL, 0);
	if (ret == -1)
	{
		perror("sendto\n");
	}
}

//lars获取某modid/cmdid的全部hosts
int lars_client::get_route(int modid, int cmdid, route_set &route)
{
	//1.封装请求的protobuf消息
	lars::GetRouteRequest req;
	req.set_modid(modid);
	req.set_cmdid(cmdid);

	//2.打包成lars能够识别的message
	char write_buf[4096], read_buf[20 * 4096];
	//消息头
	msg_head head;
	head.msglen = req.ByteSizeLong();
	head.msgid = lars::ID_API_GetRouteRequest;

	memcpy(write_buf, &head, MESSAGE_HEAD_LEN);

	req.SerializeToArray(write_buf + MESSAGE_HEAD_LEN, head.msglen);

	//3.发送
	int index = (modid + cmdid) % 3;
	int ret = sendto(_sockfd[index], write_buf, head.msglen + MESSAGE_HEAD_LEN, 0, NULL, 0);
	if (ret ==-1)
	{
		perror("send to");
		return lars::RET_SYSTEM_ERROR;
	}

	//4.阻塞等待接收数据
	int message_len;
	
	lars::GetRouteResponse rsp;
	
	message_len = recvfrom(_sockfd[index], read_buf, sizeof(read_buf), 0, NULL, 0);
	if (message_len == -1)
	{
		perror("recvfrom");
		return lars::RET_SYSTEM_ERROR;
	}

	//消息头
	memcpy(&head, read_buf, MESSAGE_HEAD_LEN);
	if (head.msgid != lars::ID_API_GetRouteResponse)
	{
		fprintf(stderr, "message ID error\n");
		return lars::RET_SYSTEM_ERROR;
	}

	//消息体
	ret = rsp.ParseFromArray(read_buf + MESSAGE_HEAD_LEN, message_len - MESSAGE_HEAD_LEN);
	if (!ret)
	{
		fprintf(stderr, "message format error\n");
		return lars::RET_SYSTEM_ERROR;
	}
	
	if (rsp.modid() != modid || rsp.cmdid() != cmdid)
	{
		fprintf(stderr, "message format error\n");
		return lars::RET_SYSTEM_ERROR;
	}

	//5.处理消息
	for (int i = 0; i < rsp.host_size(); ++i)
	{
		const lars::HostInfo &host = rsp.host(i);
		//将ip的网络字节序转换成主机字节序
		uint32_t host_ip = ntohl(host.ip());

		//ip
		struct in_addr inaddr;
		inaddr.s_addr = host_ip;
		std::string ip = inet_ntoa(inaddr);
		//port
		int port = host.port();
		//ip+port -->route
		route.push_back(ip_port(ip, port));
	}

	return lars::RET_SUCC;
}

//注册一个模块
int lars_client::reg_init(int modid, int cmdid)
{
	route_set route;

	int retry_cnt = 0;

	while (route.empty() && retry_cnt < 3)
	{
		get_route(modid,cmdid, route);

		if (route.empty() == true)
		{
			usleep(50000); //等待50ms
		}
		else
		{
			break;
		}
		++retry_cnt;
	}

	if (route.empty() == true)
	{
		return lars::RET_NOEXIST;//3
	}
	return lars::RET_SUCC;//0
}
