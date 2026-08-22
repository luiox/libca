#include "ota_image.h"

#include <em_util/crc.h>

// 校验时单次读回的分块大小：栈上分配，兼顾小 RAM MCU 与读回效率
#define OTA_IMAGE_VERIFY_CHUNK 256U

bool ota_image_header_is_valid(const ota_image_header_t *header)
{
    if (header == NULL) {
        return false;
    }
    return header->magic == OTA_IMAGE_MAGIC && header->image_size > 0;
}

u32 ota_image_total_size(const ota_image_header_t *header)
{
    if (header == NULL) {
        return 0;
    }
    return (u32)sizeof(ota_image_header_t) + header->image_size;
}

i32 ota_image_verify(const partition_t *part)
{
    if (part == NULL) {
        return OTA_IMAGE_ERR_INVALID_PARAM;
    }

    // 读回头部。分区读按字节拷贝，目的地址是对齐的结构体，无对齐问题
    ota_image_header_t header;
    i32 ret = partition_read(part, 0, (u8 *)&header, sizeof(header));
    if (ret != PARTITION_OK) {
        return OTA_IMAGE_ERR_READ_FAIL;
    }

    if (header.magic != OTA_IMAGE_MAGIC) {
        return OTA_IMAGE_ERR_MAGIC;
    }

    // 大小必须落在分区内（头之后至少容纳 image_size 字节载荷）
    if (header.image_size == 0 || header.image_size > part->size - sizeof(header)) {
        return OTA_IMAGE_ERR_SIZE;
    }

    // 分块读回载荷，增量累计 CRC32；读回而非写入时顺手累计，
    // 是为了同时验证 Flash 写入本身的正确性（擦除遗漏、位翻转）
    u32 crc      = 0;
    u32 offset   = (u32)sizeof(header);
    u32 remaining = header.image_size;
    u8  chunk[OTA_IMAGE_VERIFY_CHUNK];

    while (remaining > 0) {
        u32 len = remaining > sizeof(chunk) ? (u32)sizeof(chunk) : remaining;
        ret = partition_read(part, offset, chunk, len);
        if (ret != PARTITION_OK) {
            return OTA_IMAGE_ERR_READ_FAIL;
        }
        crc = crc32_ieee_ex(chunk, len, crc);
        offset += len;
        remaining -= len;
    }

    return (crc == header.crc32) ? OTA_IMAGE_OK : OTA_IMAGE_ERR_CRC;
}
