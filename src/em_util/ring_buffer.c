#include "ring_buffer.h"

// 队列重置
void ringbuffer_reset(ringbuffer_t* ringBuf)
{
    // 队首指针跟队尾指针都指向buffer的首个地址
    ringBuf->head = 0;
    ringBuf->tail = 0;
}

// 创建队列
void ringbuffer_init(ringbuffer_t* ringBuf, uint16_t capacity, uint8_t* buffer)
{
    // RingBufferTypeDef *ringBuf = (RingBufferTypeDef *)malloc(sizeof(RingBufferTypeDef));
    // Buffer内存申请成功
    ringBuf->buffer = buffer;   // (uint8_t *)malloc(ringBuf->size);
    ringBuf->head   = 0;
    ringBuf->tail   = 0;
    // 尺寸size需要比容量capacity大一
    ringBuf->size = capacity + 1;
    // 数值初始化置为0
    for (uint16_t i = 0; i < ringBuf->size; i++) { ringBuf->buffer[i] = 0; }
    // printf("[RingBuffer_Init] ring buffer init pbuffer=%p\n", ringBuf->buffer);
}

// 销毁队列
// void RingBuffer_Destory(RingBufferTypeDef *ringBuf){
//	free(ringBuf->buffer);
//	free(ringBuf);
//}

// 获得环形队列的容量
uint16_t ringbuffer_get_capacity(ringbuffer_t* ringBuf)
{
    return ringBuf->size - 1;
}

// 获取环形队列已经存好的字节数
uint16_t ringbuffer_get_used_bytes_size(ringbuffer_t* ringBuf)
{
    if (ringBuf->head > ringBuf->tail) {
        return ringbuffer_get_capacity(ringBuf) - (ringBuf->head - ringBuf->tail - 1);
    }
    else {
        return ringBuf->tail - ringBuf->head;
    }
}

// 获取队列的剩余容量
uint16_t ringbuffer_get_free_bytes_size(ringbuffer_t* ringBuf)
{
    return ringbuffer_get_capacity(ringBuf) - ringbuffer_get_used_bytes_size(ringBuf);
}

// 判断环形队列是否为空
uint8_t ringbuffer_is_empty(ringbuffer_t* ringBuf)
{
    return ringbuffer_get_free_bytes_size(ringBuf) == ringbuffer_get_capacity(ringBuf);
}

// 判断环形队列是否已满
uint8_t ringbuffer_is_full(ringbuffer_t* ringBuf)
{
    return ringbuffer_get_free_bytes_size(ringBuf) == 0;
}

// 根据索引号读取第i个元素
uint8_t ringbuffer_get_value_by_index(ringbuffer_t* ringBuf, uint16_t index)
{
    if (index >= ringbuffer_get_used_bytes_size(ringBuf)) {
        // 索引号超出了范围
        return 0;
    }
    uint16_t rbIdx = (ringBuf->head + index + 1) % ringBuf->size;
    return ringBuf->buffer[rbIdx];
}

// 弹出队首元素
uint8_t ringbuffer_pop(ringbuffer_t* ringBuf)
{
    uint8_t temp;
    if (ringbuffer_is_empty(ringBuf)) { return 0; }
    else {
        ringBuf->head                  = (ringBuf->head + 1) % ringBuf->size;
        temp                           = ringBuf->buffer[ringBuf->head];
        ringBuf->buffer[ringBuf->head] = 0;
        return temp;
    }
}


// 写入队尾元素
void ringbuffer_push(ringbuffer_t* ringBuf, uint8_t value)
{
    // printf("[RingBuffer_Push] before detect isFull\n");
    if (ringbuffer_is_full(ringBuf)) {
        // 队列已经写满了, 只能先弹出队首的元素
        ringbuffer_pop(ringBuf);
    }
    ringBuf->tail = (ringBuf->tail + 1) % ringBuf->size;
    // printf("[RingBuffer_Push]  ringBuf tail index: %d\n", ringBuf->tail);
    ringBuf->buffer[ringBuf->tail] = value;
    // printf("[RingBuffer_Push] tail %d\n", ringBuf->buffer[ringBuf->tail]);
}

// 读取单个字节
uint8_t ringbuffer_read_byte(ringbuffer_t* ringBuf)
{
    return ringbuffer_pop(ringBuf);
}

// 读取字节数组
void ringbuffer_read_byte_array(ringbuffer_t* ringBuf, uint8_t* dest, uint16_t size)
{
    for (uint16_t idx = 0; idx < size; idx++) { dest[idx] = ringbuffer_pop(ringBuf); }
}

// 读取有符号Short整数(两个字节)
int16_t ringbuffer_read_short(ringbuffer_t* ringBuf)
{
    int16_t  value;
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_read_byte_array(ringBuf, p, 2);
    return value;
}

// 读取无符号Short整数(两个字节)
uint16_t ringbuffer_read_ushort(ringbuffer_t* ringBuf)
{
    uint16_t value;
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_read_byte_array(ringBuf, p, 2);
    return value;
}

// 读取有符号Long类型的整数(四个字节)
int32_t ringbuffer_read_long(ringbuffer_t* ringBuf)
{
    int32_t  value;
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_read_byte_array(ringBuf, p, 4);
    return value;
}

// 读取无符号Long类型的整数(四个字节)
uint32_t ringbuffer_read_ulong(ringbuffer_t* ringBuf)
{
    uint32_t value;
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_read_byte_array(ringBuf, p, 4);
    return value;
}

// 读取浮点数(四个字节)
float ringbuffer_read_float(ringbuffer_t* ringBuf)
{
    float    value;
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_read_byte_array(ringBuf, p, 4);
    return value;
}

// 写入单个字节
void ringbuffer_write_byte(ringbuffer_t* ringBuf, uint8_t value)
{
    ringbuffer_push(ringBuf, value);
}

// 写入字节数组
void ringbuffer_write_byte_array(ringbuffer_t* ringBuf, uint8_t* src, uint16_t size)
{
    for (uint16_t idx = 0; idx < size; idx++) { ringbuffer_push(ringBuf, src[idx]); }
}
// 写入有符号Short整数(两个字节)
void ringbuffer_write_short(ringbuffer_t* ringBuf, int16_t value)
{
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_write_byte_array(ringBuf, p, 2);
}

// 写入无符号Short整数(两个字节)
void ringbuffer_write_ushort(ringbuffer_t* ringBuf, uint16_t value)
{
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_write_byte_array(ringBuf, p, 2);
}

// 写入有符号Long类型的整数(四个字节)
void ringbuffer_write_long(ringbuffer_t* ringBuf, int32_t value)
{
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_write_byte_array(ringBuf, p, 4);
}

// 写入无符号Long类型的整数(四个字节)
void ringbuffer_write_ulong(ringbuffer_t* ringBuf, uint32_t value)
{
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_write_byte_array(ringBuf, p, 4);
}

// 写入浮点数(四个字节)
void ringbuffer_write_float(ringbuffer_t* ringBuf, float value)
{
    uint8_t* p = (uint8_t*)&value;
    ringbuffer_write_byte_array(ringBuf, p, 4);
}

// 计算校验和
uint8_t ringbuffer_get_checksum(ringbuffer_t* ringBuf)
{
    uint16_t nByte = ringbuffer_get_used_bytes_size(ringBuf);
    uint32_t bSum  = 0;
    // printf("[RingBuffer_GetChecksum] \r\n");
    // 设置指针初始化位置

    for (int i = 0; i < nByte; i++) {
        // 指针逐次右移
        bSum = (bSum + ringbuffer_get_value_by_index(ringBuf, i)) % 256;
    }
    //计算校验和
    return bSum;
}
