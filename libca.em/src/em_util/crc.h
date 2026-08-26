/// @file crc.h
/// @brief 常用的CRC校验算法实现。
/// 带 _fast 前缀的函数为查表实现（空间换时间），不带 _fast 前缀的为直接计算实现。
/// 接口形式为 (data, size)，返回计算得到的 CRC 值。
#ifndef LIBCA_EM_UTIL_CRC_H
#define LIBCA_EM_UTIL_CRC_H

#include <em_base/datatype.h>

// CRC-32/IEEE 802.3
// 多项式: 0x04C11DB7 (x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1)
u32 crc32_ieee(const void* data, usize size);
u32 crc32_ieee_fast(const void* data, usize size);
/// @brief CRC-32/IEEE 滚动计算版本，用于数据分块到达、无法一次性读入的场景（如 OTA 镜像校验）
/// @param data 本块数据指针
/// @param size 本块长度（字节）
/// @param previous_crc 上一次返回的 CRC 值；首块传 0
/// @return 累计到本块为止的 CRC 值；把返回值作为下一块的 previous_crc 继续传入即可
/// @note crc32_ieee_ex(data, size, 0) 与 crc32_ieee_fast(data, size) 结果一致；
///       分块滚动计算与整段一次计算结果一致。查表实现。
u32 crc32_ieee_ex(const void* data, usize size, u32 previous_crc);

// CRC-16/MODBUS
// 多项式: 0x8005 (x^16 + x^15 + x^2 + 1)
#define CRC16_MODEBUS_START_VALUE 0xFFFF
u16 crc16_modbus(const void* data, usize size);
u16 crc16_modbus_fast(const void* data, usize size);
// ex版本，用于滚动计算
u16 crc16_modbus_ex(const void* data, usize size, u16 previous_crc);
u16 crc16_modbus_ex_fast(const void* data, usize size, u16 previous_crc);

// CRC-16/XMODEM
// 多项式: 0x1021 (x^16 + x^12 + x^5 + 1)
u16 crc16_xmodem(const void* data, usize size);
u16 crc16_xmodem_fast(const void* data, usize size);

// CRC-16/YMODEM
// 多项式: 0x1021 (x^16 + x^12 + x^5 + 1)
u16 crc16_ymodem(const void* data, usize size);
u16 crc16_ymodem_fast(const void* data, usize size);

// u8的checksum计算
u8 checksum_calc_u8(const u8* data, usize len);

#endif // !LIBCA_EM_UTIL_CRC_H
