#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "String.hpp"
#include <iostream>
#include <cstring>
#include <doctest/doctest.h>
using namespace std;
using namespace libca;

TEST_CASE("CharTest")
{
    uint8_t* cstr = (uint8_t*)"中";
    Char     c1(cstr);
    REQUIRE(strlen((char*)c1.cStr()) == 3);
}

TEST_CASE("CharsTest") {}

TEST_CASE("CharIteratorTest") {}

TEST_CASE("BytesTest") {}

TEST_CASE("ByteIterator")
{
    uint8_t      arr[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ByteIterator it(arr);
    int          i = 0;
    for (; it != arr + 10; ++it) {
        REQUIRE(*it == arr[i]);
        i++;
    }
}

TEST_CASE("StringTest")
{
    String s1;
    REQUIRE(s1.isEmpty());

    // 测试构造函数
    String s2 = String::createFromCStr("test,中文");
    REQUIRE(s2.length() == 7);
    REQUIRE(s2.byteLength() == 11);
    REQUIRE(*s2.at(0) == 't');
    REQUIRE(*s2.at(1) == 'e');
    REQUIRE(*s2.at(2) == 's');
    REQUIRE(*s2.at(3) == 't');
    REQUIRE(*s2.at(4) == ',');

    // 测试移动构造
    String s3 = std::move(s2);
    REQUIRE(s3.length() == 7);
    REQUIRE(s3.byteLength() == 11);
    REQUIRE(*s3.at(0) == 't');
    REQUIRE(*s3.at(1) == 'e');
    REQUIRE(*s3.at(2) == 's');

    // 深拷贝
    String s4 = s3.clone();
    REQUIRE(s4.length() == 7);
    REQUIRE(s4.byteLength() == 11);
    REQUIRE(*s4.at(0) == 't');
    REQUIRE(*s4.at(1) == 'e');
    REQUIRE(s4.rawData() != s3.rawData());

    // 赋值
    String s5;
    REQUIRE(s5.isEmpty());
    REQUIRE(s5.capacity() == LongStrDefaultLen);
    s5 = s4;
    REQUIRE(s5.length() == 7);
    REQUIRE(s5.byteLength() == 11);
    REQUIRE(*s5.at(0) == 't');
    REQUIRE(*s5.at(1) == 'e');
    REQUIRE(s5.rawData() != s4.rawData());

    // c风格字符串导出
    auto cstr = s5.cStr();
    REQUIRE(cstr[0] == 't');
    REQUIRE(cstr[1] == 'e');
    REQUIRE(cstr[11] == '\0');

    // 测试获取字符下标
    auto z = s5.atU(5);
    auto w = s5.atU(6);
    REQUIRE(memcmp(z, "中", 3) == 0);
    REQUIRE(memcmp(w, "文", 3) == 0);


    // auto zstr = "中";
    // REQUIRE(s2.at(5) == zstr[0]);
    // REQUIRE(s2.at(6) == zstr[1]);
    // auto z = s2.atU(5);
    // REQUIRE(*z == zstr[0]);
    // REQUIRE(*(z + 1) == zstr[1]);
    // REQUIRE(*(z+2) == zstr[1]);
}

int main_func()
{
    const char* str = u8"test,中文";
    // 打印十六进制
    printf("str hex: ");
    for (auto i = 0; i < strlen(str); i++) {
        printf("%02x ", static_cast<uint8_t>(*(str + i)));
    }
    printf("\n");

    String s1 = String::createFromCStr(str);
    // 打印十六进制
    printf("s1 hex: ");
    for (auto i = 0; i < s1.byteLength(); i++) {
        printf("%02x ", s1.at(i));
    }
    printf("\n");

    // printf("s1 hex by iterator: ");
    // for (auto ch : s1.bytes()) {
    //     printf("%s ", ch.cStr());
    // }
    // printf("\n");

    // 打印字符
    printf("s1 ch by at: ");
    for (auto i = 0; i < s1.length(); i++) {
        auto ptr = s1.atU(i);
        auto ch  = String::createFromUtf8(ptr, BytesInUtf8Char(*ptr));
        printf("%s ", ch.cStr());
    }
    printf("\n");

    printf("s1 ch by iterator: ");
    for (auto ch : s1.chars()) {
        printf("%s ", ch.cStr());
    }
    printf("\n");

    cout << "字符串：" << s1.cStr() << endl;
    cout << "字节个数：" << s1.byteLength() << endl;
    cout << "字符个数：" << s1.length() << endl;
    cout << "at(1): " << *s1.atU(1) << endl;
    cout << "byteAt(1): " << s1.at(1) << endl;
    auto ch = String::createFromUtf8((uint8_t*)s1.atU(5), 3);
    cout << "ch:" << ch << endl;
    printf("at(5): %s\n", ch.cStr());
    printf("byteAt(5): %X\n", s1.at(5));



    return 0;
}