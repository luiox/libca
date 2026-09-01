/* ota_image 单元测试：纯内存模拟，不依赖任何传输协议 */
#include "ota_image.h"

#include <em_test/test.h>
#include <em_util/crc.h>
#include <em_base/memory_util.h>

/* =========================================================================
 * 模拟 Flash 与分区表（同 test_partition.c 布局）
 * ========================================================================= */

#define MOCK_FLASH_SIZE (256 * 1024)
#define MOCK_FLASH_BASE 0x08000000
#define MOCK_ERASE_VALUE 0xFF

static u8 g_mock_flash[MOCK_FLASH_SIZE];

static i32 mock_read(u32 addr, u8* buf, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_cpy(buf, &g_mock_flash[offset], len);
    return 0;
}

static i32 mock_write(u32 addr, const u8* data, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_cpy(&g_mock_flash[offset], data, len);
    return 0;
}

static i32 mock_erase(u32 addr, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_set(&g_mock_flash[offset], MOCK_ERASE_VALUE, len);
    return 0;
}

static const partition_port_t g_mock_port = {
    .read  = mock_read,
    .write = mock_write,
    .erase = mock_erase,
};

static const partition_t g_test_partitions[] = {
    {"bootloader", 0x08000000, 0x00004000, PARTITION_FLAG_READONLY},
    {"app",
     0x08004000,
     0x00020000,
     PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE},
    {"download",
     0x08024000,
     0x00010000,
     PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE},
};

static const partition_t* find_download(void)
{
    return partition_find(g_test_partitions, 3, "download");
}

static void setup_flash(void)
{
    partition_register_port(&g_mock_port);
    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);
}

/* =========================================================================
 * 镜像构造辅助
 * ========================================================================= */

#define PAYLOAD_SIZE 1200U

// 构造一个合法镜像头，crc 字段由调用方按需填充
static ota_image_header_t make_header(void)
{
    ota_image_header_t hdr;
    hdr.magic         = OTA_IMAGE_MAGIC;
    hdr.image_size    = PAYLOAD_SIZE;
    hdr.crc32         = 0;
    hdr.version_major = 1;
    hdr.version_minor = 0;
    hdr.timestamp     = 0x67000000;
    return hdr;
}

// 组装完整镜像 blob（头+载荷），载荷为递增字节并回填 crc
static usize make_image_blob(u8* blob, usize blob_cap)
{
    ota_image_header_t hdr = make_header();
    TEST_ASSERT(blob_cap >= ota_image_total_size(&hdr));
    for (u32 i = 0; i < PAYLOAD_SIZE; i++) {
        blob[sizeof(hdr) + i] = (u8)(i * 7 + 3);
    }
    hdr.crc32 = crc32_ieee_fast(&blob[sizeof(hdr)], PAYLOAD_SIZE);
    mem_cpy(blob, &hdr, sizeof(hdr));
    return ota_image_total_size(&hdr);
}

// 把镜像 blob 按给定分块边界经 partition_stream 写入分区，模拟数据分块到达
static void stream_write_in_chunks(const partition_t* part, const u8* blob, usize blob_len,
                                   const usize* chunk_sizes, usize chunk_count)
{
    partition_stream_t stream;
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, partition_stream_open(&stream, part, (u32)blob_len));

    usize written = 0;
    for (usize i = 0; i < chunk_count && written < blob_len; i++) {
        usize len = chunk_sizes[i];
        if (written + len > blob_len) {
            len = blob_len - written;
        }
        TEST_ASSERT_EQUAL_INT(PARTITION_OK,
                              partition_stream_write(&stream, blob + written, (u32)len));
        written += len;
    }
    if (written < blob_len) {
        TEST_ASSERT_EQUAL_INT(
            PARTITION_OK,
            partition_stream_write(&stream, blob + written, (u32)(blob_len - written)));
    }

    TEST_ASSERT_EQUAL_UINT(blob_len, partition_stream_offset(&stream));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, partition_stream_close(&stream));
}

/* =========================================================================
 * 基础单元测试
 * ========================================================================= */

TEST_CASE(ota_image_header_check)
{
    ota_image_header_t hdr = make_header();
    TEST_ASSERT_TRUE(ota_image_header_is_valid(&hdr));
    TEST_ASSERT_EQUAL_UINT(sizeof(ota_image_header_t) + PAYLOAD_SIZE, ota_image_total_size(&hdr));

    // 魔数错误
    hdr.magic = 0xDEADBEEF;
    TEST_ASSERT_FALSE(ota_image_header_is_valid(&hdr));

    // 大小为 0 非法
    hdr            = make_header();
    hdr.image_size = 0;
    TEST_ASSERT_FALSE(ota_image_header_is_valid(&hdr));

    // NULL 安全
    TEST_ASSERT_FALSE(ota_image_header_is_valid(NULL));
    TEST_ASSERT_EQUAL_UINT(0, ota_image_total_size(NULL));
}

TEST_CASE(ota_image_verify_null_param)
{
    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_ERR_INVALID_PARAM, ota_image_verify(NULL));
}

TEST_CASE(ota_image_verify_ok)
{
    setup_flash();

    u8    blob[2048];
    usize blob_len = make_image_blob(blob, sizeof(blob));

    // 整段一次写入后校验通过
    const partition_t* dl = find_download();
    TEST_ASSERT_NOT_NULL(dl);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, partition_erase(dl));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, partition_write(dl, 0, blob, (u32)blob_len));
    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_OK, ota_image_verify(dl));
}

TEST_CASE(ota_image_verify_chunked_arrival)
{
    setup_flash();

    u8    blob[2048];
    usize blob_len = make_image_blob(blob, sizeof(blob));

    // 不规则分块到达（对齐真实传输中帧边界与镜像布局无关的事实），
    // 覆盖：跨界写、跨校验分块边界、末尾零碎块
    static const usize chunks[] = {1U, 33U, 1024U, 5U, 256U, 100U};
    const partition_t* dl       = find_download();
    partition_erase(dl);
    stream_write_in_chunks(dl, blob, blob_len, chunks, 6);

    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_OK, ota_image_verify(dl));

    // 读回逐字节比对
    u8 readback[2048];
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, partition_read(dl, 0, readback, (u32)blob_len));
    TEST_ASSERT_EQUAL_MEMORY(blob, readback, blob_len);
}

TEST_CASE(ota_image_verify_bad_magic)
{
    setup_flash();

    u8    blob[2048];
    usize blob_len = make_image_blob(blob, sizeof(blob));

    const partition_t* dl = find_download();
    partition_erase(dl);
    partition_write(dl, 0, blob, (u32)blob_len);

    // 破坏魔数首字节
    u8 garbage = 0x00;
    partition_write(dl, 0, &garbage, 1);
    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_ERR_MAGIC, ota_image_verify(dl));
}

TEST_CASE(ota_image_verify_bad_crc)
{
    setup_flash();

    u8    blob[2048];
    usize blob_len = make_image_blob(blob, sizeof(blob));

    const partition_t* dl = find_download();
    partition_erase(dl);
    partition_write(dl, 0, blob, (u32)blob_len);

    // 破坏载荷中部一个字节：CRC 必须查出（同时覆盖跨 OTA_IMAGE_VERIFY_CHUNK 边界）
    u8 flipped = (u8)(~blob[sizeof(ota_image_header_t) + 500]);
    partition_write(dl, sizeof(ota_image_header_t) + 500, &flipped, 1);
    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_ERR_CRC, ota_image_verify(dl));
}

TEST_CASE(ota_image_verify_bad_size)
{
    setup_flash();

    ota_image_header_t hdr = make_header();
    // 谎报大小超出分区容量（download 为 64KB）
    hdr.image_size = 0x00100000;

    const partition_t* dl = find_download();
    partition_erase(dl);
    partition_write(dl, 0, (const u8*)&hdr, sizeof(hdr));
    TEST_ASSERT_EQUAL_INT(OTA_IMAGE_ERR_SIZE, ota_image_verify(dl));
}
