#ifndef LIBCA_CRYPTO_MD5_HPP
#define LIBCA_CRYPTO_MD5_HPP

#include <string>
#include <vector>

//计算文件md5
std::string calculateMD5(const std::string& filePath);
//字节流哈希摘要
std::string calculateMD5(const std::vector<unsigned char>& data);
//对字符串进行哈希摘要
// std::string calculateMD5(const TCHAR* inputParam);

#endif // !LIBCA_CRYPTO_MD5_HPP
