#include <libca/queue.h>
#include <stdlib.h>

// 初始化队列
void queue_init(queue_t* queue)
{
    doubly_linked_list_init(queue);
}

// 加入一个元素到对位
void queue_push(queue_t* queue, void* data)
{
    doubly_linked_list_node_t* node = doubly_linked_list_node_create(data);
    if(NULL == node) return;
    doubly_linked_list_push_back(queue, node);
}

// 弹出队头元素
void queue_pop(queue_t* queue)
{
    doubly_linked_list_node_t* head = doubly_linked_list_pop_front(queue);
    if(NULL == head) return;
    free(head);
}

// 获取队头元素
void* queue_front(queue_t* queue)
{
    doubly_linked_list_node_t* head = queue->head;
    if(NULL == head) {
        return NULL;
    }
    return head->data;
}

// 获取队列大小
int32_t queue_size(queue_t* queue)
{
    return queue->size;
}

// 判断队列是否为空
bool queue_empty(queue_t* queue)
{
    return queue->size == 0;
}

// 清空队列
void queue_clear(queue_t* queue)
{
    doubly_linked_list_node_t* node =queue->head;
    while(NULL != node){
        doubly_linked_list_node_t* next = node->next;
        free(node);
        node = next;
    }
    queue->size = 0;
}
