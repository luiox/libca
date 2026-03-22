#include "memory_util.h"

#if USE_CUSTOM_MEMORY_UTIL_IMPL

void* mem_set(void* dest, u8 val, usize size)
{
    if (!dest) {
        return NULL;
    }

    u8* p = (u8*)dest;
    while (size--) {
        *p++ = val;
    }
    return dest;
}

void* mem_cpy(void* restrict dest, const void* restrict src, usize size)
{
    if (!dest || !src) {
        return dest;
    }

    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    while (size--) {
        *d++ = *s++;
    }
    return dest;
}

void* mem_move(void* dest, const void* src, usize size)
{
    if (!dest || !src || size == 0) {
        return dest;
    }

    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    if (d < s) {
        // 从头向尾拷贝
        while (size--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        // 从尾向头拷贝，处理重叠
        d += size - 1;
        s += size - 1;
        while (size--) {
            *d-- = *s--;
        }
    }

    return dest;
}

i32 mem_cmp(const void* s1, const void* s2, usize size)
{
    if (s1 == s2 || size == 0) {
        return 0;
    }
    if (!s1) return -1;
    if (!s2) return 1;

    const u8* p1 = (const u8*)s1;
    const u8* p2 = (const u8*)s2;

    while (size--) {
        if (*p1 != *p2) {
            return (i32)(*p1 - *p2);
        }
        p1++;
        p2++;
    }

    return 0;
}

void* mem_find_byte(const void* buf, u8 val, usize size)
{
    if (!buf) {
        return NULL;
    }

    const u8* p = (const u8*)buf;
    while (size--) {
        if (*p == val) {
            return (void*)p;
        }
        p++;
    }

    return NULL;
}

#endif /* USE_CUSTOM_MEMORY_UTIL_IMPL */

