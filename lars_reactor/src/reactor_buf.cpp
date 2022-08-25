#include "reactor_buf.h"
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


reactor_buf::reactor_buf()
{
	_buf = NULL;
}

reactor_buf::~reactor_buf()
{
	this->clear();
}

//已经处理了多少数据
void reactor_buf::pop(int len)
{
	assert(_buf != NULL && len <= _buf->length);

	_buf->pop(len);

	if (_buf->length == 0)
	{
		//当前的_buf已经全部用完了
		this->clear();
	}
}

//将当前的_buf清空
void reactor_buf::clear()
{
	if (_buf != NULL)
	{
		//将_buf放回buf_pool中
		buf_pool::instance()->revert(_buf);
		_buf = NULL;
	}
}

//获得当前buf还有多少有效数据
int reactor_buf::length()
{
	if (_buf == NULL) return 0;
	else
	{
		return _buf->length;
	}
}


//========================================================

//从一个fd中读取数据到reactor_buf中
int input_buf::read_data(int fd)
{
	int need_read; //硬件中有多少数据

	//一次性将io中的缓存数据全部读出来
	//需要给fd设置一个属性
	//传出一个参数，目前缓冲中一共有多少数据是可读的
	if (ioctl(fd, FIONREAD, &need_read) == -1)
	{
		fprintf(stderr, "ioctl FIONREAD\n");
		return -1;
	}

	if (_buf == NULL)
	{
		//如果当前的input_buf里的_buf是空，需要从buf_pool拿一个新的
		_buf = buf_pool::instance()->alloc_buf(need_read);
		if (_buf ==NULL)
		{
			fprintf(stderr, "no buf for alloc\n");
			return -1;
		}
	}
	else
	{
		//如果当前buf可用，判断一下当前buf是否够存
		assert(_buf->head == 0);
		if (_buf->capacity - _buf->length < need_read)
		{
			//不够存，重新申请一个buf
			io_buf *new_buf = buf_pool::instance()->alloc_buf(need_read + _buf->length);
			if (new_buf ==NULL)
			{
				fprintf(stderr, "no buf for alloc\n");
				return -1;
			}
		
			//将之前的_buf数据拷贝到新的buf中
			new_buf->copy(_buf);

			//将之前的_buf放回内存池中
			buf_pool::instance()->revert(_buf);

			//新申请的buf称为当前的io_buf
			_buf = new_buf;
		}	
	}

	int already_read = 0;

	//当前的buf是可以容纳 读取数据
	do
	{
		if (need_read == 0)
		{
			already_read = read(fd, _buf->data + _buf->length, m4K);//阻塞直到有数据
		}
		else
		{	
			already_read = read(fd, _buf->data + _buf->length, need_read);
		}
	}while(already_read == -1 && errno == EINTR);//systemcall一个终端，良性，需要继续读取

	if (already_read > 0)
	{
		if (need_read != 0)
		{
			assert(already_read == need_read);
		}
		
		//读数据已经成功
		_buf->length += already_read;
	}

	return already_read;
}

//获取当前的数据的方法
const char *input_buf::data()
{
	return _buf != NULL ? _buf->data + _buf->head : NULL;
}

//重置缓冲区
void input_buf::adjust()
{
	if (_buf != NULL)
	{
		_buf->adjust();
	}
}



//=============================================


//将_buf中的数据写到fd中
int output_buf:: write2fd(int fd) //取代io层 write方法
{
	assert(_buf != NULL && _buf->head ==0);

	int already_write = 0;
	do
	{
		already_write = write(fd, _buf->data, _buf->length);
	}while (already_write == -1 && errno == EINTR);//系统调用中断，不是错误

	if (already_write > 0)
	{
		//已经写成功
		_buf->pop(already_write);
		_buf->adjust();
	}

	//如果fd是非阻塞的，会报already_write == -1 errno==EAGAIN
	if (already_write == -1 && errno == EAGAIN)
	{
		already_write = 0;//表示非阻塞导致的-1不是一个错误，表示是正确的只是写0个字节
	}

	return already_write;
}

//将一段数据写到自身的_buf中
int output_buf::send_data(const char *data, int datalen)
{
	if (_buf == NULL)
	{
		//如果当前的output_buf里的_buf是空，需要从buf_pool拿一个新的
		_buf = buf_pool::instance()->alloc_buf(datalen);
		if (_buf ==NULL)
		{
			fprintf(stderr, "no buf for alloc\n");
			return -1;
		}
	}
	else
	{
		//如果当前buf可用，判断一下当前buf是否够存
		assert(_buf->head == 0);
		if (_buf->capacity - _buf->length < datalen)
		{
			//不够存，重新申请一个buf
			io_buf *new_buf = buf_pool::instance()->alloc_buf(datalen + _buf->length);
			if (new_buf ==NULL)
			{
				fprintf(stderr, "no buf for alloc\n");
				return -1;
			}
		
			//将之前的_buf数据拷贝到新的buf中
			new_buf->copy(_buf);

			//将之前的_buf放回内存池中
			buf_pool::instance()->revert(_buf);

			//新申请的buf设置为当前的io_buf
			_buf = new_buf;
		}
	}

	//将data数据写到io_buf中 拼接到后面
	memcpy(_buf->data + _buf->length, data, datalen);

	_buf->length += datalen;

	return 0;
}
