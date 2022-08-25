#pragma once


/*
*定义一个buffer内存块的数据结构体
*/

class io_buf
{
public:
	//构造， 创建一个size大小的buf
	io_buf(int size);

	//清空数据
	void clear();

	//处理长度为len的数据，移动head
	void pop(int len); //len表示已经处理的数据的长度

	//将已经处理的数据从内存中清空，将未处理的数据移至buf的首地址，length会减少
	void adjust();

	//将其他的io_buf对象拷贝到自己中
	void copy(const io_buf *other);
	
	//当前buf的总容量
	int capacity;

	//当前buf的有效数据长度
	int length;

	//当前未处理有效数据的头部索引
	int head;

	//当前buf的内存首地址
	char* data;

	//存在多个io_buf采用链表的形式进行管理
	io_buf *next;
};
