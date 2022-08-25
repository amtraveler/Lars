#include "dns_route.h"
#include "config_file.h"
#include <string>
#include "subscribe.h"

using namespace std;

//单例初始化
Route *Route::_instance = NULL;
pthread_once_t Route::_once = PTHREAD_ONCE_INIT;


Route::Route():_version(0)
{
	printf("Route init\n");
	
	memset(_sql, 0, 1000);

	//初始化锁
	pthread_rwlock_init(&_map_lock, NULL);

	//初始化map
	_data_pointer = new route_map();
	_temp_pointer = new route_map();

	//链接数据库
	this->connect_db();

	//将数据库中的RouteData的数据加载到_data_pointer中
	this->build_maps();
}


//链接数据库的方法
void Route::connect_db()
{
	//加载mysql的配置参数
	string db_host = config_file::instance()->GetString("mysql", "db_host", "127.0.0.1");
	short db_port = config_file::instance()->GetNumber("mysql", "db_port", 3306);
	string db_user = config_file::instance()->GetString("mysql", "db_user", "root");
	string db_passwd = config_file::instance()->GetString("mysql", "db_passwd", "dell12345678");
	string db_name= config_file::instance()->GetString("mysql", "db_name", "lar_dns");

	//初始化mysql链接
	mysql_init(&_db_conn);
	
	//设置一个超时定期重连
	mysql_options(&_db_conn, MYSQL_OPT_CONNECT_TIMEOUT, "30");
	//开启mysql断开连接自动重连机制
	my_bool reconnect = 1;
	mysql_options(&_db_conn, MYSQL_OPT_RECONNECT, &reconnect);

	//链接数据库
	if (!mysql_real_connect(&_db_conn, db_host.c_str(), db_user.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0))
	{
		fprintf(stderr, "Faild to connect mysql\n");
		exit(1);
	}

	printf("connect db succ!\n");

}

//将RouteData表中数据加载到内存map中
void Route::build_maps()
{
	int ret = 0;
	memset(_sql, 0, 1000);
	//查询RouteData数据库
	snprintf(_sql, 1000, "SELECT * FROM RouteData;");
	ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
	if (ret != 0)
	{
		fprintf(stderr, "select RouteData error %s\n", mysql_error(&_db_conn));
		exit(1);
	}
	
	//或得一个结果结合
	MYSQL_RES *result = mysql_store_result(&_db_conn);

	//得到行数
	long line_num = mysql_num_rows(result);

	//遍历分析集合中的元素，加入_data_pointer中（MapA中）
	MYSQL_ROW row;
	for (int i = 0; i < line_num; ++i)
	{
		//处理一行的数据
		row = mysql_fetch_row(result);
		int modID = atoi(row[1]);
		int cmdID = atoi(row[2]);
		unsigned int ip = atoi(row[3]);
		int port = atoi(row[4]);
		
		printf("modid = %d, cmdid = %d, ip = %u, port = %d\n", modID, cmdID, ip, port);

		//将读到的数据加入map中
		//组装一个map的key
		uint64_t key = ((uint64_t)modID << 32) + cmdID;
		uint64_t value = ((uint64_t)ip <<32) + port;

		(*_data_pointer)[key].insert(value);
	}
	mysql_free_result(result);
}

//将RouteData表中数据加载到内存_temp_pointer map中
void Route::load_route_data()
{
	int ret = 0;
	memset(_sql, 0, 1000);

	//清空temp_pointer所指向的临时表
	_temp_pointer->clear();

	//查询RouteData数据库
	snprintf(_sql, 1000, "SELECT * FROM RouteData;");
	ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
	if (ret != 0)
	{
		fprintf(stderr, "select RouteData error %s\n", mysql_error(&_db_conn));
		exit(1);
	}
	
	//或得一个结果结合
	MYSQL_RES *result = mysql_store_result(&_db_conn);

	//得到行数
	long line_num = mysql_num_rows(result);

	//遍历分析集合中的元素，加入_data_pointer中（MapA中）
	MYSQL_ROW row;
	for (int i = 0; i < line_num; ++i)
	{
		//处理一行的数据
		row = mysql_fetch_row(result);
		int modID = atoi(row[1]);
		int cmdID = atoi(row[2]);
		unsigned int ip = atoi(row[3]);
		int port = atoi(row[4]);

		//将读到的数据加入map中
		//组装一个map的key
		uint64_t key = ((uint64_t)modID << 32) + cmdID;
		uint64_t value = ((uint64_t)ip <<32) + port;

		(*_temp_pointer)[key].insert(value);
	}
	mysql_free_result(result);
}

//通过modid/cmdid获取全部的当前模块所挂载的host集合
host_set Route::get_hosts(int modid, int cmdid)
{
	host_set hosts;

	//组装key
	uint64_t key = ((uint64_t)modid << 32) + cmdid;

	//通过map来得到
	pthread_rwlock_rdlock(&_map_lock);
	route_map_it it = _data_pointer->find(key);
	if (it != _data_pointer->end())
	{
		//找到了对应的host set
		hosts = it->second;
	}

	pthread_rwlock_unlock(&_map_lock);
	return hosts;
}

//将_temp_pointer的数据更新到_data_pointer中
void Route::swap()
{
	pthread_rwlock_wrlock(&_map_lock);

	route_map *temp = _data_pointer;
	_data_pointer = _temp_pointer;
	_temp_pointer = temp; 

	pthread_rwlock_unlock(&_map_lock);
}

//周期性后端检查db的route信息的更改业务
void *check_route_changes(void *args)
{
	int wait_time = 10; //10s自动加载RouteData一次
	long last_load_time = time(NULL);

	while (1)
	{
		sleep(1);
		long current_time = time(NULL);//当前时间

		//业务1 判断版本是否已经被修改
		int ret = Route::instance()->load_version();
		if (ret == 1)
		{
			//version已经被更改，有modid/cmdid被修改

			//1 将最新的RouteData的数据加载到_temp_data中
			Route::instance()->load_route_data();

			//2 将_temp_pointer的数据，更新到_data_pointer中
			Route::instance()->swap();
			last_load_time = current_time;

			//3 获取当前已经被修改的modid/cmdid集合vector
			std::vector<uint64_t> changes;
			Route::instance()->load_changes(changes);

			//4 给订阅修改的mod客户端agent推送消息
			SubscribeList::instance()->publish(changes);

			//5 TODO 将RouteChanges表清空
		}
		else
		{
			//version没有被修改
			if (current_time - last_load_time >= wait_time)
			{
				//定期检查超时，强制加载_temp_pointer--->_data_pointer中
				Route::instance()->load_route_data();
				Route::instance()->swap();

				last_load_time = current_time;

			}
		}


	}
	return NULL;
}

//test发布的函数
void *publish_change_mod_test(void *args)
{
	while (1)
	{
		sleep(1);
		
		//modid=1，cmdid=1被修改
		int modid1 = 1;
		int cmdid1 = 1;
		uint64_t mod1 = (((uint64_t)modid1) << 32) + cmdid1;

		//modid=1,cmdid=2被修改
		int modid2 = 1;
		int cmdid2 = 2;
		uint64_t mod2 = (((uint64_t)modid2) << 32) + cmdid2;

		std::vector<uint64_t> changes;
		changes.push_back(mod1);
		changes.push_back(mod2);

		SubscribeList::instance()->publish(changes);
	}
}

//加载当前版本
//return 0 成功 version没有改变
//return 1 成功 version有改变
//return -1 失败
int Route::load_version()
{
	memset(_sql, 0, 1000);
	snprintf(_sql, 1000, "SELECT version from RouteVersion WHERE id = 1;");
	
	int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
	if (ret != 0)
	{
		fprintf(stderr, "select RouteData error %s\n", mysql_error(&_db_conn));
		exit(1);
	}

	//获得一个结果结合
	MYSQL_RES *result = mysql_store_result(&_db_conn);

	//得到行数
	long line_num = mysql_num_rows(result);
	if (line_num == 0)
	{
		fprintf(stderr, "No version in table RouteVersion: %s\n", mysql_error(&_db_conn));
		return -1;
	}

	MYSQL_ROW row = mysql_fetch_row(result);

	//得到最新的version
	long new_version = atoi(row[0]);

	if (new_version == this->_version)
	{
		return 0;
	}

	this->_version = new_version;

	printf("now route version is %ld\n", this->_version);

	mysql_free_result(result);

	return 1;
}

//加载RoutChange得到修改的modid/cmdid
//放在vector中 
void Route::load_changes(std::vector<uint64_t>& change_list)
{
	memset(_sql, 0, 1000);

	snprintf(_sql, 1000, "SELECT modid, cmdid from RouteChange WHERE version >= %ld;", this->_version);

	int ret = mysql_real_query(&_db_conn, _sql, strlen(_sql));
	if (ret != 0)
	{
		fprintf(stderr, "select RouteChamge error %s\n", mysql_error(&_db_conn));
		exit(1);
	}

	//获得一个结果结合
	MYSQL_RES *result = mysql_store_result(&_db_conn);

	//得到行数
	long line_num = mysql_num_rows(result);
	if (line_num == 0)
	{
		fprintf(stderr, "No change in RouteChange: %s\n", mysql_error(&_db_conn));
		return ;
	}

	MYSQL_ROW row;
	for (long i = 0; i < line_num; ++i)
	{
		row = mysql_fetch_row(result);
		int modid = atoi(row[0]);
		int cmdid = atoi(row[1]);

		uint64_t mod = ((uint64_t)modid << 32) + cmdid;
		change_list.push_back(mod);
	}

	mysql_free_result(result);

}
