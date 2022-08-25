#include "io_buf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>


//构造， 创建一个size大小的buf
io_buf::io_buf(int size)
{
	capacity = size;
	length = 0;
	head = 0;
	next = NULL;

	data = new char[capacity];
	//如果data为空，程序直接退出
	assert(data);
}

//清空数据
void io_buf::clear()
{
	length = head = 0;
}

//处理长度为len的数据，移动head
void io_buf::pop(int len) //len表示已经处理的数据的长度
{
	length -= len;
	head += len;
}

//将已经处理的数据从内存中清空，将未处理的数据移至buf的首地址，length会减少
void io_buf::adjust()
{
	if (head != 0)
	{
		//length ==0 代表全部的数据以及处理完
		if(length != 0)
		{
			memmove(data, data+head, length);
		}
		head = 0;
	}
}

//将其他的io_buf对象拷贝到自己中
void io_buf::copy(const io_buf *other)
{
	memcpy(data, other->data + other->head, other->length);
	head = 0;
	length = other->length;
}

