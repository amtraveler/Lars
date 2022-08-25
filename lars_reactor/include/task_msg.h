#pragma once
#include "event_loop.h"


/*
*thread_queue消息队列 所能接受的消息类型
*
*/


struct task_msg
{
	//有两大类型
	//一、新建立链接的任务
	//二、一般的普通任务 主thread希望分发一些任务给每个线程处理

	enum TASK_TYPE
	{
		NEW_CONN,//新建链接的任务
		NEW_TASK //一般的任务
	};

	TASK_TYPE type;//任务类型

	//任务本身数据内容
	union
	{
		//如果是任务1，task_msg的任务内容就是一个刚刚创建好的connfd
		int connfd;

		//如果是任务2，task_msg的任务内容应该由具体的数据参数和具体的回调业务
		struct
		{
			void (*task_cb)(event_loop *loop, void *args);
			void *args;
		};
	};
};
