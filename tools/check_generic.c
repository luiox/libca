#include <stdio.h>

// 宏定义测试
#define TYPE_NAME(x) _Generic((x), \
    int: "int", \
    float: "float", \
    double: "double", \
    char*: "string", \
    default: "unknown" \
)

int main() {
    int a = 10;
    float b = 3.14f;
    char* c = "hello";

    printf("a is %s\n", TYPE_NAME(a));
    printf("b is %s\n", TYPE_NAME(b));
    printf("c is %s\n", TYPE_NAME(c));
    printf("100 is %s\n", TYPE_NAME(100));

    return 0;
}
