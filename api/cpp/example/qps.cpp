#include "lars_api.h"
#include <iostream>

struct ID
{
	int t_id; //线程ID
	int modid;
	int cmdid;
};

void usage()
{
	printf("usage : ./qps [thread_num]\n");
}

void *test_qps(void *args)
{
	ID* id = (ID*)args;
	int modid = id->modid;
	int cmdid = id->cmdid;

	int ret = 0;
	lars_client api;
	std::string ip;
	int port;

	//qps访问记录
	long qps = 0;
	long last_time = time(NULL);

	long total_qps = 0;
	long total_time_second = 0;
	
	//1 将modid/cmdid注册（只调用一次）
	ret = api.reg_init(modid, cmdid);
	if (ret != 0)
	{
		std::cout << "modid " << modid << " cmdid " << cmdid << " not exist, register error ret = " << ret << std::endl;
	}
	
	while (1)
	{
		//2.测试获取主机
		ret = api.get_host(modid, cmdid, ip, port);
		if (ret == 0 || ret == 1 || ret == 3)
		{
			++qps;//增加一次响应数量
			if (ret == 0)
			{
				//上报ip+port 结果
				api.report(modid, cmdid, ip, port, 0); //上报成功
			}
			else if (ret == 1)
			{
				//上报ip+port 结果
				api.report(modid, cmdid, ip, port, 1);//上报过载
			}
		}
		else
		{
			//RET_SYSTEM_ERROR
			printf("[%d, %d] get error ret = %d\n", modid, cmdid, ret);
		}

		//获取当前的时间
		long current_time = time(NULL);
		if (current_time - last_time >= 1)
		{
			//统计
			total_time_second += 1;
			total_qps += qps;
			last_time = current_time;

			printf("thread %d ---> qps = %ld, average = %ld\n", id->t_id, qps, total_qps / total_time_second);
			qps = 0;
		}
	}

	return NULL;
}

//开发者业务模块
int main(int argc, char** argv)
{
	if (argc != 2)
	{
		usage();
		return 1;
	}

	int cnt = atoi(argv[1]);
	ID* ids = new ID[cnt];
	pthread_t *tids = new pthread_t[cnt];

	for (int i = 0; i < cnt; ++i)
	{
		ids[i].t_id = i;
		ids[i].modid = i + 1;
		ids[i].cmdid = 1;

		pthread_create(&tids[i], NULL, test_qps, &ids[i]);
	}
	
	for (int i = 0; i < cnt; ++i)
	{
		pthread_join(tids[i], NULL);
	}

	return 0;
}
