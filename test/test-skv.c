#include "component/skv.h"
#include "base/string_util.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

skv_kv_t kvs[10];
skv_t    skv;
char     buf[1024];
char     value[SKV_MAX_VALUE_LEN];

void test_basic_operations() {
    printf("Testing basic operations...\n");
    
    // 初始化测试
    skv_init(&skv, kvs, 10);
    
    // 插入测试
    bool result = skv_put(&skv, "name", "skv");
    assert(result == true);
    
    // 查询测试
    result = skv_get(&skv, "name", value);
    assert(result == true);
    assert(strcmp(value, "skv") == 0);
    
    // 更新测试
    result = skv_put(&skv, "name", "skv_updated");
    assert(result == true);
    
    // 查询更新后的值
    result = skv_get(&skv, "name", value);
    assert(result == true);
    assert(strcmp(value, "skv_updated") == 0);
    
    printf("Basic operations test passed.\n");
}

void test_serialization() {
    printf("Testing serialization...\n");
    
    // 序列化
    skv_to_str(&skv, buf);
    
    // 写到文件里面看看
    FILE* f = fopen("skv.txt", "w");
    if (f) {
        fputs(buf, f);
        fclose(f);
    }
    
    // 清空当前数据
    skv_init(&skv, kvs, 10);
    
    // 反序列化
    skv_from_str(&skv, buf);
    
    // 验证数据是否正确恢复
    bool result = skv_get(&skv, "name", value);
    assert(result == true);
    assert(strcmp(value, "skv_updated") == 0);
    
    printf("Serialization test passed.\n");
}

void test_multiple_entries() {
    printf("Testing multiple entries...\n");
    
    // 添加多个键值对
    assert(skv_put(&skv, "key1", "value1") == true);
    assert(skv_put(&skv, "key2", "value2") == true);
    assert(skv_put(&skv, "key3", "value3") == true);
    
    // 验证所有键值对
    assert(skv_get(&skv, "key1", value) == true);
    assert(strcmp(value, "value1") == 0);
    
    assert(skv_get(&skv, "key2", value) == true);
    assert(strcmp(value, "value2") == 0);
    
    assert(skv_get(&skv, "key3", value) == true);
    assert(strcmp(value, "value3") == 0);
    
    printf("Multiple entries test passed.\n");
}

void test_capacity_limit() {
    printf("Testing capacity limit...\n");
    
    // 填满存储
    for (int i = 0; i < 10; i++) {
        char key[16], val[16];
        sprintf(key, "key%d", i);
        sprintf(val, "val%d", i);
        assert(skv_put(&skv, key, val) == true);
    }
    
    // 尝试添加超出容量的键值对
    assert(skv_put(&skv, "overflow", "should_fail") == false);
    
    printf("Capacity limit test passed.\n");
}

void test_edge_cases() {
    printf("Testing edge cases...\n");
    
    // 查询不存在的键
    assert(skv_get(&skv, "nonexistent", value) == false);
    
    // 测试空键
    assert(skv_put(&skv, "", "empty_key") == true);
    assert(skv_get(&skv, "", value) == true);
    assert(strcmp(value, "empty_key") == 0);
    
    printf("Edge cases test passed.\n");
}

void test_bin_save()
{
    // 测试写入到二进制文件
        // 初始化测试
    skv_init(&skv, kvs, 10);
    // 添加多个键值对
    assert(skv_put(&skv, "key1", "value1") == true);
    assert(skv_put(&skv, "key2", "value2") == true);
    assert(skv_put(&skv, "key3", "value3") == true);
    skv_to_str(&skv, buf);
    FILE* fp = fopen("skv.bin", "wb");
    // 先写出字符串的长度u32
    u32 len = str_len(buf) + 1;
    fwrite(&len, sizeof(u32), 1, fp);
    // 写出字符串
    fwrite(buf, sizeof(u8), len, fp);
    

}

int main() {
    printf("Starting SKV tests...\n");
    
    test_basic_operations();
    test_serialization();
    test_multiple_entries();
    test_capacity_limit();
    test_edge_cases();
    test_bin_save();
    printf("All tests passed!\n");
    return 0;
}