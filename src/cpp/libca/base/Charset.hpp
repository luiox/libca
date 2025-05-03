// 用于提供字符编码操作
#pragma once

#include "Platform.hpp"
#include <vector>
#include "String.hpp"

namespace ca {
    
class Charset
{
public:
    enum Encoding
    {
        Latin1,
        Gbk,
        Gb2312,
        Utf8,
        Utf16,
        Utf32
    };

    static std::vector<u8> encode(Encoding encoding, const std::vector<u8>& str);
};

////////////////////////////////////////////////////////////////////////////////

// 字符串在不同平台上的一些处理，尤其是Window平台，处理各种字符串的转换
// 这个类纯粹是一个工具类，不包含任何数据
class LIBCA_API StringConverter
{
public:
    StringConverter()  = delete;
    ~StringConverter() = delete;

    // string转wstring
    static std::wstring stringToWideString(const std::string& narrowStr);

    // wstring转string
    static std::string wideStringToString(const std::wstring& wideStr);

    // wstring转本地string
    static std::string wideStringToString2(const std::wstring& wideStr);

    // wchar_t*转string
    static std::string wcharToString(const wchar_t* str);

    // wchar_t*转wstring
    static std::wstring wcharToWideString(const wchar_t* wcharStr);

    // char*转wchar_t*
    static std::wstring charToWchar(const char* charStr);

    // gbk转utf8
    static std::string gbkToUtf8(const std::string& gbkString);

    // 将 utf8 编码的字符串转换为 GBK 编码
    static std::string utf8ToGbk(const std::string& utf8String);

    // 将 utf8 编码的字符串转换为 Unicode 编码
    static std::wstring utf8ToUnicode(const std::string& utf8String);

    // Unicode转Utf8
    static std::string unicodeToUtf8(const std::wstring& unicodeString);

    // 本地代码页转std::wstring
    static std::wstring localCodePageToWstring(const std::string& str);

    // 本地代码页转std::string
    static std::string localCodePageToUtf8(const std::string& localString);

    static std::u16string mstrToU16str(String& str);

    static std::u32string mstrToU32str(String& str);

    static std::wstring mstrUtf8ToWstrUtf16(String& utf8str);

    static String wstrUtf16ToMstrUtf8(wchar_t* utf16str);
};

}   // namespace ca