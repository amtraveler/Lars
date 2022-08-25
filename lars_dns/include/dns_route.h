#pragma once
#include <pthread.h>
#include <stdio.h>
#include "mysql.h"
#include <ext/hash_map>
#include <ext/hash_set>
#include <stdint.h>
#include <string.h>

/*
* 表示modid/cmdid --> host:ip, host:port的对应关系
*/

//用来保存host ip/port的集合类型
typedef __gnu_cxx::hash_set<uint64_t> host_set;
typedef __gnu_cxx::hash_set<uint64_t>::iterator host_set_it;

//定义用来保存modID/cmdID 与IP/port的对应的数据类型
typedef __gnu_cxx::hash_map<uint64_t, host_set> route_map;
typedef __gnu_cxx::hash_map<uint64_t, host_set>::iterator route_map_it;



//全局唯一 不能够增加，也不能够减少，将唯一对象先初始化好
class Route
{
public:
	//创建一个单例的方法
	static void init()
	{
		_instance = new Route();
	}

	//获取当前的单例
	static Route *instance()
	{
		pthread_once(&_once, init);
		return _instance;
	}

	//链接数据库的方法
	void connect_db();

	//构建map route数据的fangfa
	//将RouteData表中数据加载到内存_data_pointer map中
	void build_maps();

	//将RouteData表中数据加载到内存_temp_pointer map中
	void load_route_data();

	//通过modid/cmdid获取全部的当前模块所挂载的host集合
	host_set get_hosts(int modid, int cmdid);

	//加载当前版本
	//return 0 成功 version没有改变
	//return 1 成功 version有改变
	//return -1 失败
	int load_version();

	//加载RoutChange得到修改的modid/cmdid
	//放在vector中 
	void load_changes(std::vector<uint64_t>& change_list);

	//将_temp_pointer的数据更新到_data_pointer中
	void swap();
private:
	//全部函数私有化
	Route();
	Route(const Route &);
	const Route& operator=(const Route&);

	//单例锁
	static pthread_once_t _once;

	//指向当前单例对象的指针
	static Route *_instance;

	//mysql数据库的属性
	MYSQL _db_conn;

	route_map *_data_pointer;//指向RouteDataMap_A 当前关系map
	route_map *_temp_pointer;//指向RouteDataMap_B 临时关系map

	pthread_rwlock_t _map_lock; //map读写锁

	//当前的版本号
	long _version;

	//
	char _sql[1000];
};

void *publish_change_mod_test(void *args);
void *check_route_changes(void *args);
