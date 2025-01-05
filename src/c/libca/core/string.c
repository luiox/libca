#include <libca/core/string.h>
#include <string.h>
#include <stdlib.h>

char* str_trim(char* str)
{
    if (str == NULL)
        return NULL;
    // 找到字符串开始的非空白字符
    char* p_start = str;
    while (*p_start != '\0' && (*p_start == ' ' || *p_start == '\t' || *p_start == '\n')) {
        p_start++;
    }

    // 找到字符串末尾的非空白字符
    char* p_end = str + strlen(str) - 1;
    while (p_end > p_start && (*p_end == ' ' || *p_end == '\t' || *p_end == '\n')) {
        p_end--;
    }

    // 将非空白字符移动到字符串的开始，并在末尾添加字符串终止符
    memmove(str, p_start, p_end - p_start + 1);
    str[p_end - p_start + 1] = '\0';

    return str;
}
