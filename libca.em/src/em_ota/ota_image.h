/// @file ota_image.h
/// @brief OTA 镜像格式定义与完整性校验（读回比对）。
///
/// 镜像布局 = ota_image_header_t + 载荷。头由上位机拼接在 bin 前随流写入分区，
/// MCU 侧不区分头与载荷，传输完成后用 ota_image_verify() 做整镜像读回校验。
/// 设计细节见 em_ota设计文档.md 的「OTA 升级流程层设计」。
#ifndef LIBCA_EM_OTA_OTA_IMAGE_H
#define LIBCA_EM_OTA_OTA_IMAGE_H

#include <em_base/datatype.h>

#include "partition.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 镜像魔数，字节序为小端 "OTA0"
#define OTA_IMAGE_MAGIC 0x3054414FU

/// 错误码：成功
#define OTA_IMAGE_OK (0)
/// 错误码：参数非法（如 part 为 NULL）
#define OTA_IMAGE_ERR_INVALID_PARAM (-1)
/// 错误码：魔数不匹配（分区起始不是合法镜像头）
#define OTA_IMAGE_ERR_MAGIC (-2)
/// 错误码：镜像大小非法（为 0 或超出分区容量）
#define OTA_IMAGE_ERR_SIZE (-3)
/// 错误码：CRC 校验失败（内容损坏或写入不完整）
#define OTA_IMAGE_ERR_CRC (-4)
/// 错误码：底层读失败
#define OTA_IMAGE_ERR_READ_FAIL (-5)

/// @brief 镜像头（20 字节，字段自然对齐，无需 pack）
///
/// 字段布局跨编译器一致；未来格式演进通过 magic 版本号区分（"OTA1"...），
/// 不预留 reserved 字段。
typedef struct ota_image_header
{
    u32 magic;           ///< 固定魔数 OTA_IMAGE_MAGIC
    u32 image_size;      ///< 载荷字节数（不含头）
    u32 crc32;           ///< 载荷 CRC32（crc32_ieee 算法，不含头）
    u16 version_major;   ///< 语义化版本主号，仅记录用
    u16 version_minor;   ///< 语义化版本次号，仅记录用
    u32 timestamp;       ///< 构建 Unix 时间戳，可为 0
} ota_image_header_t;

/// @brief 判断镜像头是否合法
/// @param header 待检查的头（原始字节读出后直接转型即可）
/// @retval true  魔数匹配且 image_size > 0
/// @retval false 魔数不匹配或大小为 0
bool ota_image_header_is_valid(const ota_image_header_t* header);

/// @brief 计算镜像总大小（头 + 载荷），用于 stream_open 的 total_size
/// @param header 已填充的镜像头
/// @return 头与载荷的总字节数
u32 ota_image_total_size(const ota_image_header_t* header);

/// @brief 对写入完成的分区做完整性校验（读回比对，同时验证 Flash 写入正确性）
///
/// 流程：读回头部校验 magic/size → 分块读回载荷增量累计 CRC32 → 与头部 crc32 比对。
/// 内部使用栈上 256 字节分块缓冲，不动态分配内存。
///
/// @param part 已写入完整镜像（头+载荷、自偏移 0 开始）的目标分区
/// @return OTA_IMAGE_OK 成功；其余错误码见宏定义
i32 ota_image_verify(const partition_t* part);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_OTA_OTA_IMAGE_H
