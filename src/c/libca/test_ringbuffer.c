#include <libca/ringbuffer.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

// 定义一个环形缓冲区实例
#define BUFFER_SIZE RINGBUFFER_SIZE_GEN(8)
uint8_t      buffer[BUFFER_SIZE];
ringbuffer_t rb;

// 用于测试的辅助函数
void testInitialization()
{
    ringbuffer_init(&rb, buffer, BUFFER_SIZE);
    assert(ringbuffer_get_data_size(&rb) == 0);
    assert(ringbuffer_get_free_size(&rb) == BUFFER_SIZE);
}

void testWriteReadSingleByte()
{
    uint8_t writeData = 0xAA;
    uint8_t readData  = 0;

    ringbuffer_reset(&rb);
    assert(ringbuffer_write(&rb, &writeData, 1));
    assert(ringbuffer_get_data_size(&rb) == 1);
    assert(ringbuffer_get_free_size(&rb) == BUFFER_SIZE - 1);
    assert(ringbuffer_read(&rb, &readData, 1));
    assert(readData == writeData);
}

void testWriteReadMultiByte()
{
    uint8_t writeData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t readData[4];

    ringbuffer_reset(&rb);
    assert(ringbuffer_write(&rb, writeData, 4));
    assert(ringbuffer_get_data_size(&rb) == 4);
    assert(ringbuffer_get_free_size(&rb) == BUFFER_SIZE - 4);
    assert(ringbuffer_read(&rb, readData, 4));
    assert(memcmp(writeData, readData, 4) == 0);
}

void testWriteUntilFull()
{
    uint8_t writeData[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        writeData[i] = (uint8_t)i;
    }

    ringbuffer_reset(&rb);
    assert(ringbuffer_write(&rb, writeData, BUFFER_SIZE));
    assert(ringbuffer_get_data_size(&rb) == BUFFER_SIZE);
    assert(ringbuffer_get_free_size(&rb) == 0);
}

void testReadWriteLoop()
{
    uint8_t writeData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t readData[4];
    int     iterations = BUFFER_SIZE / sizeof(writeData);

    ringbuffer_reset(&rb);
    for (int i = 0; i < iterations; ++i) {
        assert(ringbuffer_write(&rb, writeData, sizeof(writeData)));
    }
    assert(ringbuffer_get_data_size(&rb) == BUFFER_SIZE);
    assert(ringbuffer_get_free_size(&rb) == 0);

    for (int i = 0; i < iterations; ++i) {
        assert(ringbuffer_read(&rb, readData, sizeof(readData)));
        assert(memcmp(writeData, readData, sizeof(writeData)) == 0);
    }
    assert(ringbuffer_get_data_size(&rb) == 0);
    assert(ringbuffer_get_free_size(&rb) == BUFFER_SIZE);
}

int main()
{
    testInitialization();
    testWriteReadSingleByte();
    testWriteReadMultiByte();
    testWriteUntilFull();
    testReadWriteLoop();

    printf("All tests passed.\n");
    return 0;
}