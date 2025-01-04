#include <libca/ringbuffer.h>
#include <assert.h>

/**
 * @brief 初始化环形缓冲区.
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区，要求是可用的内存，且大小为2的幂次方
 * @param size 缓冲区大小
 */
void ringbuffer_init(ringbuffer_t* rb, uint8_t* buffer, postion_size_t size)
{
	assert(rb);
	assert(buffer);
	rb->buffer = buffer;
	rb->size = size;
	rb->used = 0;
	rb->read = 0;
	rb->write = 0;
}

/**
 * @brief 重置环形缓冲区.
 * @param rb 环形缓冲区指针
 */
void ringbuffer_reset(ringbuffer_t* rb)
{
	assert(rb);
	rb->read = 0;
	rb->write = 0;
	rb->used = 0;
}

/**
 * @brief 往环形缓冲区里写数据.
 * @param rb 环形缓冲区指针
 * @param data 指向数据的指针
 * @param size 写入的数据大小
 * @return 是否写入成功
 */
bool ringbuffer_write(ringbuffer_t* rb, uint8_t* data, postion_size_t size)
{
	assert(rb);
	assert(data);
	if (ringbuffer_get_free_size(rb) < size) {
		// 缓冲区容量不够
		return false;
	}
	// 写入数据
	for (postion_size_t i = 0; i < size; i++) {
		rb->buffer[rb->write] = data[i];
		// 设置写指针位置
		// 如果可以的话确保这是原子的操作，否则可能会导致数据不一致
		rb->write = (rb->write + 1) & (rb->size - 1); 
	}
	rb->used += size;
	return true;
}

/**
 * @brief 从环形缓冲区里读数据.
 * @param rb 环形缓冲区指针
 * @param buf 指向读取缓冲区的指针
 * @param size 读取的数据大小
 * @param 是否读取成功
 */
bool ringbuffer_read(ringbuffer_t* rb, uint8_t* buf, postion_size_t size)
{
	assert(rb);
	assert(buf);
	if (ringbuffer_get_data_size(rb) < size) {
		// 缓冲区内容不够读取
		return false;
	}
	// 读取数据
	for (postion_size_t i = 0; i < size; i++) {
		buf[i] = rb->buffer[rb->read];
		// 设置读指针位置
		// 如果可以的话确保这是原子的操作，否则可能会导致数据不一致
		rb->read = (rb->read + 1) & (rb->size - 1);
	}
	rb->used -= size;
	return true;
}

/**
 * @brief 获取环形缓冲区里的数据大小.
 * @param rb 环形缓冲区指针
 * @return 数据大小
 */
postion_size_t ringbuffer_get_data_size(ringbuffer_t* rb)
{
	assert(rb);
	return rb->used;
}

/**
 * @brief 获取环形缓冲区里的空闲大小.
 * @param rb 环形缓冲区指针
 * @return 空闲大小
 */
postion_size_t ringbuffer_get_free_size(ringbuffer_t* rb)
{
	assert(rb);
	return rb->size - rb->used;
}
