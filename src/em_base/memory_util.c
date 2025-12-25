#include "memory_util.h"

#define ALIGNMENT 8   // 假设我们要求8字节对齐

typedef struct block
{
    size_t        size;
    struct block* next;
    int           free;
} block_t;

block_t* free_list = NULL;


static void*    heap_buf;
static uint32_t heap_size;

void heap_init(void* buf, uint32_t size)
{
    heap_buf  = buf;
    heap_size = size;

    // 初始化空闲列表
    free_list       = (block_t*)buf;
    free_list->size = size - sizeof(block_t);
    free_list->next = NULL;
    free_list->free = 1;
}

void* heap_alloc(uint32_t size)
{
    block_t* current    = free_list;
    block_t* prev       = NULL;
    size_t   total_size = size + sizeof(block_t);

    // 确保请求的大小是8字节对齐的
    if (total_size % ALIGNMENT != 0) {
        total_size += ALIGNMENT - (total_size % ALIGNMENT);
    }

    while (current) {
        if (current->free && current->size >= total_size) {
            // 找到一个足够大的空闲块
            if (current->size > total_size + sizeof(block_t)) {
                // 如果块足够大，我们可以分割它
                block_t* new_block = (block_t*)((char*)current + total_size);
                new_block->size    = current->size - total_size;
                new_block->next    = current->next;
                new_block->free    = 1;
                current->size      = total_size;
                current->next      = new_block;
            }
            current->free = 0;
            return (void*)(current + 1);   // 返回用户数据部分
        }
        prev    = current;
        current = current->next;
    }

    // 没有找到足够大的空闲块
    return NULL;
}

void heap_free(void* ptr)
{
    if (!ptr) {
        return;
    }

    block_t* current = (block_t*)ptr - 1;   // 转换回块头部
    current->free    = 1;

    // 尝试合并前一个空闲块
    block_t* prev = free_list;
    block_t* temp = NULL;
    while (prev && (void*)prev < (void*)current) {
        temp = prev;
        prev = prev->next;
    }
    if (temp && temp->free) {
        temp->size += current->size + sizeof(block_t);
        temp->next = current->next;
        current    = temp;
    }

    // 尝试合并后一个空闲块
    while (current->next && current->next->free) {
        current->size += current->next->size + sizeof(block_t);
        current->next = current->next->next;
    }
}

// 测试代码

#if 0
// 假设我们有4k字节的内存区域
char memory[4*1024];

int main()
{
    // 初始化堆
    heap_init(memory, sizeof(memory));

    // 分配内存
    void *ptr1 = heap_alloc(100);
    if (ptr1) {
        printf("Allocated 100 bytes\n");
    }

    // 再次分配内存
    void *ptr2 = heap_alloc(200);
    if (ptr2) {
        printf("Allocated 200 bytes\n");
    }

    // 释放内存
    heap_free(ptr1);
    printf("Freed 100 bytes\n");

    // 再次分配内存
    void *ptr3 = heap_alloc(150);
    if (ptr3) {
        printf("Allocated 150 bytes\n");
    }

    // 释放所有内存
    heap_free(ptr2);
    heap_free(ptr3);
}

#endif
