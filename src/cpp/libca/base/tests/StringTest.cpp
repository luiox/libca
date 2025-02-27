#include "String.hpp"
#include <iostream>
#include <cstring>
using namespace std;
using namespace libca;

int main()
{
    const char* str = u8"test,中文";
    // 打印十六进制
    printf("str hex: ");
    for (auto i = 0; i < strlen(str); i++) {
        printf("%02x ", static_cast<uint8_t>(*(str + i)));
    }
    printf("\n");

    String s1 = String::createFromUtf8(str, strlen(str));
    // 打印十六进制
    printf("s1 hex: ");
    for (auto i = 0; i < s1.byteLength(); i++) {
        printf("%02x ", s1.byteAt(i));
    }
    printf("\n");

    // 打印字符
    printf("s1 ch by at: ");
    for (auto i = 0; i < s1.length(); i++) {
        auto ptr = s1.at(i);
        auto ch = String::createFromUtf8(reinterpret_cast<const char*>(ptr), BytesInUtf8Char(*ptr));
        printf("%s ", ch.cStr());
    }
    printf("\n");

    printf("s1 ch by iterator: ");
    for (auto ch : s1) {
        printf("%s ", ch.cStr());
    }
    printf("\n");

    cout << "字符串：" << s1.cStr() << endl;
    cout << "字节个数：" << s1.byteLength() << endl;
    cout << "字符个数：" << s1.length() << endl;
    cout << "at(1): " << *s1.at(1) << endl;
    cout << "byteAt(1): " << s1.byteAt(1) << endl;
    auto ch = String::createFromUtf8(reinterpret_cast<const char*>(s1.at(5)), 3);
    printf("at(5): %s\n", ch.cStr());
    printf("byteAt(5): %X\n", s1.byteAt(5));

    return 0;
}