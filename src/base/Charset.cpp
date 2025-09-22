#include "Charset.hpp"
#include <codecvt>
#include <libiconv/iconv.h>
namespace ca {


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 编码转换，source_charset是源编码，to_charset是目标编码
std::string code_convert(char* source_charset, char* to_charset,
                         const std::string& sourceStr)   // sourceStr是源编码字符串
{
    iconv_t cd = iconv_open(to_charset, source_charset);   // 获取转换句柄，void*类型
    if (cd == 0)
        return "";

    size_t inlen  = sourceStr.size();
    size_t outlen = 255;
    char*  inbuf  = (char*)sourceStr.c_str();
    char   outbuf[255];   // 这里实在不知道需要多少个字节，这是个问题
    // char *outbuf = new char[outlen];
    // 另外outbuf不能在堆上分配内存，否则转换失败，猜测跟iconv函数有关
    memset(outbuf, 0, outlen);

    char* poutbuf =
        outbuf;   // 多加这个转换是为了避免iconv这个函数出现char(*)[255]类型的实参与char**类型的形参不兼容
    if (iconv(cd, &inbuf, &inlen, &poutbuf, &outlen) == -1)
        return "";

    std::string strTemp(outbuf);   // 此时的strTemp为转换编码之后的字符串
    iconv_close(cd);
    return strTemp;
}

// gbk转UTF-8
std::string GbkToUtf8(const std::string& strGbk)   // 传入的strGbk是GBK编码
{
    return code_convert("gb2312", "utf-8", strGbk);
}

// UTF-8转gbk
std::string Utf8ToGbk(const std::string& strUtf8)
{
    return code_convert("utf-8", "gb2312", strUtf8);
}

// gbk转unicode,"UCS-2LE"代表unicode小端模式
std::string GbkToUnicode(const std::string& strGbk)   // 传入的strGbk是GBK编码
{
    return code_convert("gb2312", "UCS-2LE", strGbk);
}

// unicode转gbk
std::string UnicodeToGbk(const std::string& strGbk)   // 传入的strGbk是GBK编码
{
    return code_convert("UCS-2LE", "gb2312", strGbk);
}

// string转wstring
std::wstring StringConverter::stringToWideString(const std::string& narrowStr)
{
    // 获取宽字符字符串的长度（包括空终止符）
    int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, nullptr, 0);

    // 分配内存来存储宽字符字符串
    wchar_t* wideStr = new wchar_t[wideStrLength];

    // 将窄字符转换为宽字符
    MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, wideStr, wideStrLength);

    // 创建 std::wstring 对象
    std::wstring result(wideStr);

    // 释放内存
    delete[] wideStr;

    return result;
}

// wstring转string
// 注意: 在Windows下将utf16转utf8的std::string是无法正常显示中文的
std::string StringConverter::wideStringToString(const std::wstring& wideStr)
{
    int bufferSize =
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
    return str;
}

// wstring转本地string
// 注意: 本地ansi可以显示中文，但请不要再网络内容传输中使用它，因为不同计算机本地代码页不相同.
std::string StringConverter::wideStringToString2(const std::wstring& wideStr)
{
    int bufferSize =
        WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
    return str;
}

// wchar_t*转string
std::string StringConverter::wcharToString(const wchar_t* str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(str);
}

// wchar_t*转wstring
std::wstring StringConverter::wcharToWideString(const wchar_t* wcharStr)
{
    // 使用构造函数将 wchar_t* 转换为 std::wstring
    std::wstring wideStr(wcharStr);

    return wideStr;
}

// char*转wchar_t*
std::wstring StringConverter::charToWchar(const char* charStr)
{
    const int charStrLength = strlen(charStr) + 1;   // char 字符串的长度（包括 null 终止符）

    // 计算 wchar_t 字符串所需的缓冲区大小
    const int wcharStrSize = MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, nullptr, 0);

    // 分配 wchar_t 缓冲区
    wchar_t* wcharStr = new wchar_t[wcharStrSize];

    // 执行转换
    MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, wcharStr, wcharStrSize);

    // 将 wchar_t 字符串封装到 std::wstring 类型
    std::wstring result(wcharStr);

    // 释放内存
    delete[] wcharStr;

    return result;
}

// gbk转utf8
std::string StringConverter::gbkToUtf8(const std::string& gbkString)
{
    int          bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
    std::wstring wideString(bufferSize - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize - 1);

    bufferSize =
        WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8String(bufferSize - 1, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize - 1, nullptr, nullptr);

    return utf8String;
}

// gbk转utf8
// std::string GbkToUTF8(const std::string& gbkString)
// {
//     int          bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr,);
//     std::wstring wideString(bufferSize, L'\0');
//     MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize);

//     bufferSize =
//         WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
//     std::string utf8String(bufferSize, '\0');
//     WideCharToMultiByte(
//         CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize, nullptr, nullptr);
//     return utf8String;
// }

// 将 utf8 编码的字符串转换为 GBK 编码
std::string StringConverter::utf8ToGbk(const std::string& utf8String)
{
    int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
    if (bufferSize == 0) {
        // 转换失败，可以根据实际情况进行错误处理
        return "";
    }

    std::wstring wideString(bufferSize, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], bufferSize);

    bufferSize =
        WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        // 转换失败，可以根据实际情况进行错误处理
        return "";
    }

    std::string gbkString(bufferSize, '\0');
    WideCharToMultiByte(
        CP_ACP, 0, wideString.c_str(), -1, &gbkString[0], bufferSize, nullptr, nullptr);

    return gbkString;
}

// 将 utf8 编码的字符串转换为 Unicode 编码
std::wstring StringConverter::utf8ToUnicode(const std::string& utf8String)
{
    int          bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
    std::wstring unicodeString(bufferSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &unicodeString[0], bufferSize);
    return unicodeString;
}

// 本地代码页转std::wstring
std::wstring StringConverter::localCodePageToWstring(const std::string& str)
{
    int wideStrLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (wideStrLen == 0) {
        // 转换失败，可以根据实际情况处理错误
        return L"";
    }

    std::wstring wideStr(wideStrLen, L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideStrLen) == 0) {
        // 转换失败，可以根据实际情况处理错误
        return L"";
    }

    // 去掉末尾的空字符
    wideStr.resize(wideStrLen - 1);

    return wideStr;
}

// 本地代码页转std::string
std::string StringConverter::localCodePageToUtf8(const std::string& localString)
{
    int wideCharLength = MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, nullptr, 0);
    if (wideCharLength == 0) {
        // 转换失败
        return "";
    }

    std::wstring wideString(wideCharLength, L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, &wideString[0], wideCharLength) ==
        0) {
        // 转换失败
        return "";
    }

    int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) {
        // 转换失败
        return "";
    }

    std::string utf8String(utf8Length, '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) ==
        0) {
        // 转换失败
        return "";
    }

    return utf8String;
}

// Unicode转Utf8
std::string StringConverter::unicodeToUtf8(const std::wstring& unicodeString)
{
    int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length == 0) {
        // 转换失败
        return "";
    }

    std::string utf8String(utf8Length, '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) ==
        0) {
        // 转换失败
        return "";
    }

    return utf8String;
}

std::u16string StringConverter::mstrToU16str(String& str)
{
    return u"";
}

std::u32string StringConverter::mstrToU32str(String& str)
{
    return U"";
}

std::wstring StringConverter::mstrUtf8ToWstrUtf16(String& utf8str)
{
    char* str = (char*)utf8str.rawData();
    if (!str)
        return L"";

    // 打开转换描述符
    iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
    if (cd == (iconv_t)-1) {
        std::cerr << "iconv_open failed" << std::endl;
        return L"";
    }

    // 估算转换后的字符串长度
    size_t   utf8_len  = utf8str.byteLength();
    size_t   wchar_len = utf8_len;                     // 最多不会超过UTF-8长度
    wchar_t* wchar_str = new wchar_t[wchar_len + 1];   // +1 for null-terminator

    // 设置转换的输入输出参数
    char*  in_buf   = const_cast<char*>(str);
    size_t in_left  = utf8_len;
    char*  out_buf  = reinterpret_cast<char*>(wchar_str);
    size_t out_left = wchar_len * sizeof(wchar_t);

    // 执行转换
    size_t result = iconv(cd, &in_buf, &in_left, &out_buf, &out_left);
    if (result == (size_t)-1) {
        std::cerr << "iconv failed" << std::endl;
        delete[] wchar_str;
        iconv_close(cd);
        return L"";
    }

    // 确保字符串以null终止
    wchar_str[wchar_len - out_left / sizeof(wchar_t)] = L'\0';

    // 关闭转换描述符
    iconv_close(cd);

    // 转换为std::wstring
    std::wstring wstr(wchar_str);
    delete[] wchar_str;
    return wstr;
}

String StringConverter::wstrUtf16ToMstrUtf8(wchar_t* utf16str)
{
    if (utf16str == nullptr) {
        return String();
    }

    // 打开转换描述符
    iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
    if (cd == (iconv_t)-1) {
        std::cerr << "iconv_open failed" << std::endl;
        return String();
    }

    // 估算转换后的字符串长度
    size_t utf16len     = wcslen(utf16str);
    size_t utf8len      = utf16len * 4;            // UTF-8最多使用4个字节表示一个字符
    char*  utf8str      = new char[utf8len + 1];   // +1 for null-terminator
    char*  utf8str_ptr  = utf8str;
    char*  utf16str_ptr = reinterpret_cast<char*>(utf16str);

    // 执行转换
    size_t result = iconv(cd, &utf16str_ptr, &utf16len, &utf8str_ptr, &utf8len);
    if (result == (size_t)-1) {
        std::cerr << "iconv failed" << std::endl;
        delete[] utf8str;
        iconv_close(cd);
        return String();
    }

    // 确保字符串以null终止
    *utf8str_ptr = '\0';

    // 关闭转换描述符
    iconv_close(cd);
    // 直接移动进去，把之后内存释放的工作交给String
    return String::createFromUtf8ByMove((u8char*)utf8str, utf8len);
}

}   // namespace ca
