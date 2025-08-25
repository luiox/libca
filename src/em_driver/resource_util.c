#include "resource_util.h"

// 资源数组，用于存储指向各种硬件资源的指针
void* resources[RESOURCE_MAX_SIZE] = {0};

/**
 * @brief 绑定资源
 *
 * @param id 资源id
 * @param res 资源指针
 */
void resource_bind(resource_id_t id, void* res)
{
    if (id < RESOURCE_MAX_SIZE) {
        resources[id] = res;
    }
}

/**
 * @brief 获取资源
 *
 * @param id 资源id
 * @return void* 资源指针
 */
void* resource_get(resource_id_t id)
{
    if (id < RESOURCE_MAX_SIZE) {
        return resources[id];
    }
    return 0;
}