/**
 * @file base64.h
 * @author canrad (1517807724@qq.com)
 * @brief Base64 编解码
 * @version 0.1
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_CRYPTO_BASE64_H
#define LIBCA_EM_CRYPTO_BASE64_H

#include <em_base/datatype.h>

/**
 * @brief 计算编码后需要的缓冲区大小
 * @param input_len 输入数据长度
 * @return 编码后需要的字符数（不含null终止符）
 */
usize base64_encode_len(usize input_len);

/**
 * @brief 将二进制数据编码为base64字符串
 * @param input 输入数据
 * @param input_len 输入长度
 * @param output 输出缓冲区（应至少为 base64_encode_len(input_len)+1 字节）
 * @return 输出字符串长度（不含null终止符），失败返回0
 */
usize base64_encode(const u8 *input, usize input_len, char *output);

/**
 * @brief 计算解码后需要的缓冲区大小
 * @param input 编码字符串
 * @param input_len 编码字符串长度（不含null终止符）
 */
usize base64_decode_len(const char *input, usize input_len);

/**
 * @brief 将base64字符串解码为二进制数据
 * @param input 输入字符串
 * @param input_len 输入长度（不含null终止符）
 * @param output 输出缓冲区（应至少为 base64_decode_len(input_len) 字节）
 * @return 实际解码出的字节数，失败返回0
 */
usize base64_decode(const char *input, usize input_len, u8 *output);

#endif // !LIBCA_EM_CRYPTO_BASE64_H
