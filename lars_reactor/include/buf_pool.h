#pragma once

#include <ext/hash_map>
#include <stdint.h>
#include "io_buf.h"

typedef __gnu_cxx::hash_map<int, io_buf*> pool_t;

//定义一些内存的刻度
enum MEM_CAP
{
	m4K = 4096,
	m16K = 16384,
	m64K = 65536,
	m256K = 262144,
	m1M = 1048576,
	m4M = 4194304,
	m8M = 8388608,
};

//总内存池的大小限制 单位是kb
#define MEM_LIMIT (5U * 1024 * 1024)

class buf_pool
{
public:
	//初始化单例对象
	static void init()
	{
		_instance = new buf_pool();
	}

	//提供一个静态的获取instance的方法
	static buf_pool* instance()
	{
		//保证init方法在进程的生命周期只执行一次
		pthread_once(&_once, init);
		return _instance;
	}

	//从内存池中申请一个内存
	io_buf *alloc_buf(int N);
	io_buf *alloc_buf();

	//重置一个io_buf放回pool中
	void revert(io_buf *buffer);

	//开辟初始使用空间
	void make_io_buf_list(int cap, int num);
private:
	//========== 创建单例 ================
	//构造函数私有化
	buf_pool();
	buf_pool(const buf_pool&);//拷贝构造
	const buf_pool& operator=(const buf_pool&);//拷贝赋值运算符
	//单例的对象
	static buf_pool *_instance;
	//用于保证创建单例的一个方法全局只执行一次
	static pthread_once_t _once;

	//========== buf_poo属性 ==============
	//存放所有io_buf的map句柄
	pool_t _pool;
	//当前内存池总体大小 单位是kb
	uint64_t _total_mem;

	//保护pool_map增删改查的锁
	static pthread_mutex_t _mutex;
};
