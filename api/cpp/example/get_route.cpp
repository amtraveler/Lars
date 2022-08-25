#include "lars_api.h"
#include <iostream>


void usage()
{
	printf("usage : ./get_route [modid] [cmdid]\n");
}


//开发者业务模块
int main(int argc, char** argv)
{
	if (argc != 3)
	{
		usage();
		return 1;
	}

	lars_client api;
	route_set route;
	int ret = 0;

	int modid = atoi(argv[1]);
	int cmdid = atoi(argv[2]);

	//1 将modid/cmdid注册（只调用一次）
	ret = api.reg_init(modid, cmdid);
	if (ret != 0)
	{
		std::cout << "modid " << modid << " cmdid " << cmdid << " not exist, register error ret = " << ret << std::endl;
	}
	
	//2 获取全部的host信息
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
