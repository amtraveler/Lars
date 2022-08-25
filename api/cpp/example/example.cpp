#include "lars_api.h"
#include <iostream>


void usage()
{
	printf("usage : ./example [modid] [cmdid]\n");
}


//开发者业务模块
int main(int argc, char** argv)
{
	if (argc != 3)
	{
		usage();
		return 1;
	}
	
	int ret = 0;
	lars_client api;
	std::string ip;
	int port;

	int modid = atoi(argv[1]);
	int cmdid = atoi(argv[2]);

	//1 将modid/cmdid注册（只调用一次）
	ret = api.reg_init(modid, cmdid);
	if (ret != 0)
	{
		std::cout << "modid " << modid << " cmdid " << cmdid << " not exist, register error ret = " << ret << std::endl;
	}

	//2.测试获取主机
	ret = api.get_host(modid, cmdid, ip, port);
	if (ret == 0)
	{
		std::cout << "host is " << ip << ":" << port << std::endl;

		//上报ip+port 结果
		api.report(modid, cmdid, ip, port, 1);
	}
	else if (ret == 3) //RET_NOEXSIT
	{
		printf("not exsit!\n");
	}

	//3.测试获取route
	route_set route;
	ret = api.get_route(modid, cmdid, route);
	if (ret == 0)
	{
		for (route_set_it it = route.begin(); it != route.end(); ++it)
		{
			std::cout << "ip " << (*it).first << ", port = " << (*it).second << std::endl;
		}
	}
	return 0;
}
