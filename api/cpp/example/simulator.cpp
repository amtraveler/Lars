#include "lars_api.h"
#include <iostream>


void usage()
{
	printf("usage : ./simulator [modid] [cmdid] [errRate(0-10)] [query cnt(0-999999)]\n");
}


//开发者业务模块
int main(int argc, char** argv)
{
	if (argc < 3)
	{
		usage();
		return 1;
	}

	int modid = atoi(argv[1]);
	int cmdid = atoi(argv[2]);
	int err_rate = 2; //20%
	int query_cnt = 100;

	if (argc > 3) {
		err_rate = atoi(argv[3]);
	}

	if (argc > 4) {
		query_cnt = atoi(argv[4]);
	}

	//key-->ip, value--> <succ_cnt, err_cnt>
	std::map<std::string, std::pair<int, int> > result;
	
	int ret = 0;
	std::string ip;
	int port;
	lars_client api;

	std::cout << "err_rate = " << err_rate <<std::endl;


	//1 将modid/cmdid注册（只调用一次）
	ret = api.reg_init(modid, cmdid);
	if (ret != 0)
	{
		std::cout << "modid " << modid << " cmdid " << cmdid << " not exist, register error ret = " << ret << std::endl;
	}

	//启动模拟器测试
	for (int i = 0; i < query_cnt; ++i)
	{
		ret = api.get_host(modid, cmdid, ip, port);
		if (ret == 0)
		{
			//获取成功
			if (result.find(ip) == result.end())
			{
				//首次获取当前ip
				std::pair<int, int> succ_err(0, 0);
				result[ip] = succ_err;
			}

			//业务
			std::cout << "host" << ip << ":" << "host called" << std::endl;

			if (rand()%10 < err_rate)
			{
				//上报错误消息
				result[ip].second += 1;
				api.report(modid, cmdid, ip, port, 1);
				std::cout << "ERROR!!!" << std::endl;
			}
			else
			{
				//上报正确的消息
				result[ip].first += 1;
				api.report(modid, cmdid, ip, port, 0);
				std::cout << "SUCC." << std::endl;
			}
		}
		else if (ret == 3)
		{
			std::cout << modid << ", " << cmdid << " not exist" << std::endl;
		}
		else if (ret == 1)
		{
			std::cout << modid << ", " << cmdid << " all hosts overload" << std::endl;
		}
		else if (ret == 2)
		{

			std::cout << "lars system error!!!" << std::endl;
		}
		usleep(5000);
	}

	std::map<std::string, std::pair<int, int> >::iterator it;
	for (it = result.begin(); it != result.end(); ++it)
	{
		std::cout << "ip " << it->first << ": ";
		std::cout << "succ " << it->second.first << ", ";
		std::cout << "error " << it->second.second << std::endl;

	}

	return 0;
}
