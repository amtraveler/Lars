#pragma once
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/*
*一个主机的基本信息
*/

struct host_info
{
	host_info(uint32_t ip, int port, int init_succ):ip(ip), port(port), vsucc(init_succ), verr(0), rsucc(0),rerr(0),contin_succ(0),contin_err(0), overload(false) 
	{
		idle_ts = time(NULL);
	}


	void set_overload();
	void set_idle();

	uint32_t ip;
	int port;
	uint32_t vsucc;//虚拟成功次数（API反馈）-->作用是过载或者空闲的判断
	uint32_t verr;//虚拟的失败次数
	uint32_t rsucc;//真实的成功次数 -->用来reporter上报，入库
	uint32_t rerr;//真实的失败次数

	uint32_t contin_succ;//连续的成功次数
	uint32_t contin_err;//连续的失败次数

	long idle_ts;//当主机更变为idle状态的时间
	long overload_ts;//当主机更变为overload状态的时间
	bool overload;//当前的状态是否过载
};
