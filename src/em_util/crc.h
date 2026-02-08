/**
 * @file crc.h
 * @brief 常用的CRC校验算法实现。
 * 不带 _fast 后缀的函数为朴素实现（多项式计算），带 _fast 后缀的为查表实现。
 * 参数均为 (data, size)，返回计算得到的 CRC 值。
 */
#ifndef LIBCA_EM_UTIL_CRC_H
#define LIBCA_EM_UTIL_CRC_H

#include "../em_base/datatype.h"

// CRC-32/IEEE 802.3
// 多项式: 0x04C11DB7 (x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1)
u32 crc32_ieee(const void* data, usize size);
u32 crc32_ieee_fast(const void* data, usize size);

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
