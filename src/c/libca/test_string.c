#include <libca/string.h>
#include <stdio.h>

void test1()
{
    char buf[512] = "  \n \t 21345\n\t ";
    printf("%s1", str_trim(buf));
}

int main(int argc, char* argv[])
{
    test1();
    return 0;
}