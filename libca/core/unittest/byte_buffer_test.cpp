#include <gmock/gmock.h>

#include "libca/core/bytes.hpp"

namespace ca::core { namespace test {

using namespace testing;

// ==================== Bytes — 构造 ====================

TEST(BytesTest, DefaultConstructor) {
    Bytes b;
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(b.len(), 0u);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, FromStatic) {
    u8 data[] = {0x01, 0x02, 0x03};
    auto b = Bytes::from_static(data, 3);
    EXPECT_EQ(b.len(), 3u);
    EXPECT_EQ(b.as_ptr(), static_cast<const u8*>(data));
}

TEST(BytesTest, CopyFromSlice) {
    u8 data[] = {0x01, 0x02, 0x03};
    auto b = Bytes::copy_from_slice(data, 3);
    EXPECT_EQ(b.len(), 3u);
    EXPECT_NE(b.as_ptr(), static_cast<const u8*>(data)); // different allocation
}

TEST(BytesTest, CopyFromSliceEmpty) {
    auto b = Bytes::copy_from_slice(nullptr, 0);
    EXPECT_TRUE(b.is_empty());
}

TEST(BytesTest, NullEmpty) {
    auto b = Bytes::from_static(nullptr, 0);
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(b.len(), 0u);
}

// ==================== Bytes — 读游标 ====================

TEST(BytesTest, ReadAndAdvance) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04};
    auto b = Bytes::from_static(data, 4);
    EXPECT_EQ(b.remaining(), 4u);
    EXPECT_EQ(b.get_u8().unwrap(), 0x01);
    EXPECT_EQ(b.remaining(), 3u);
    EXPECT_TRUE(b.advance(2).is_ok());
    EXPECT_EQ(b.remaining(), 1u);
    EXPECT_EQ(b.get_u8().unwrap(), 0x04);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, AdvancePastEndReturnsErr) {
    u8 data[] = {0x01};
    auto b = Bytes::from_static(data, 1);
    EXPECT_TRUE(b.advance(2).is_err());
    EXPECT_EQ(b.remaining(), 1u); // 游标不动
}

TEST(BytesTest, ReadPastEndReturnsErr) {
    auto b = Bytes::from_static(nullptr, 0);
    EXPECT_TRUE(b.get_u8().is_err());
}

// ==================== Bytes — 切片 ====================

TEST(BytesTest, Slice) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto b = Bytes::from_static(data, 5);
    auto s = b.slice(1, 4);
    EXPECT_EQ(s.len(), 3u);
    EXPECT_EQ(s.get_u8().unwrap(), 0x02);
    EXPECT_EQ(s.get_u8().unwrap(), 0x03);
    EXPECT_EQ(s.get_u8().unwrap(), 0x04);
}

TEST(BytesTest, SliceInvalidRange) {
    u8 data[] = {0x01, 0x02};
    auto b = Bytes::from_static(data, 2);
    EXPECT_THROW(b.slice(1, 3), std::out_of_range);
    EXPECT_THROW(b.slice(3, 1), std::out_of_range);
}

TEST(BytesTest, SliceOfSliceSharedStorage) {
    auto b = Bytes::copy_from_slice(reinterpret_cast<const u8*>("hello world"), 11);
    auto s1 = b.slice(0, 5);
    auto s2 = b.slice(6, 11);
    EXPECT_EQ(s1.len(), 5u);
    EXPECT_EQ(s2.len(), 5u);
    EXPECT_EQ(s1.get_u8().unwrap(), 'h');
    EXPECT_EQ(s2.get_u8().unwrap(), 'w');
}

// ==================== Bytes — 类型化读 ====================

TEST(BytesTest, GetU16_BigEndian) {
    u8 data[] = {0xAB, 0xCD};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_u16_be().unwrap(), 0xABCDu);
}

TEST(BytesTest, GetU16_LittleEndian) {
    u8 data[] = {0xCD, 0xAB};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_u16_le().unwrap(), 0xABCDu);
}

TEST(BytesTest, GetU32_BigEndian) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04};
    auto b = Bytes::from_static(data, 4);
    EXPECT_EQ(b.get_u32_be().unwrap(), 0x01020304u);
}

TEST(BytesTest, GetU32_LittleEndian) {
    u8 data[] = {0x04, 0x03, 0x02, 0x01};
    auto b = Bytes::from_static(data, 4);
    EXPECT_EQ(b.get_u32_le().unwrap(), 0x01020304u);
}

TEST(BytesTest, GetU64_BigEndian) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto b = Bytes::from_static(data, 8);
    EXPECT_EQ(b.get_u64_be().unwrap(), 0x0102030405060708u);
}

TEST(BytesTest, GetU64_LittleEndian) {
    u8 data[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    auto b = Bytes::from_static(data, 8);
    EXPECT_EQ(b.get_u64_le().unwrap(), 0x0102030405060708u);
}

TEST(BytesTest, GetI16) {
    u8 data[] = {0xFF, 0xF6};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_i16_be().unwrap(), -10);
}

TEST(BytesTest, GetF32) {
    u8 data[] = {0x40, 0x49, 0x0F, 0xDB}; // 3.14159f in big-endian IEEE 754
    auto b = Bytes::from_static(data, 4);
    EXPECT_NEAR(b.get_f32_be().unwrap(), 3.14159f, 1e-5f);
}

TEST(BytesTest, GetValuesSequentially) {
    u8 data[] = {0x01,           // u8  = 0x01
                 0x02, 0x03,     // u16 = 0x0203
                 0x04, 0x05, 0x06, 0x07, // u32 = 0x04050607
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // u64
    auto b = Bytes::from_static(data, 16);
    EXPECT_EQ(b.get_u8().unwrap(),    0x01u);
    EXPECT_EQ(b.get_u16_be().unwrap(),   0x0203u);
    EXPECT_EQ(b.get_u32_be().unwrap(),   0x04050607u);
    EXPECT_EQ(b.get_u64_be().unwrap(),   0x08090A0B0C0D0E0Fu);
}

// ==================== Bytes — 批量读 ====================

TEST(BytesTest, CopyToSlice) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04};
    auto b = Bytes::from_static(data, 4);
    u8 dst[3];
    EXPECT_TRUE(b.copy_to_slice(dst, 3).is_ok());
    EXPECT_EQ(dst[0], 0x01);
    EXPECT_EQ(dst[1], 0x02);
    EXPECT_EQ(dst[2], 0x03);
    EXPECT_EQ(b.remaining(), 1u); // advanced
}

TEST(BytesTest, CopyToSliceUnderflow) {
    u8 data[] = {0x01};
    auto b = Bytes::from_static(data, 1);
    u8 dst[3];
    EXPECT_TRUE(b.copy_to_slice(dst, 3).is_err());
    EXPECT_EQ(b.remaining(), 1u); // 游标不动
}


// ==================== BytesMut — 构造 ====================

TEST(BytesMutTest, DefaultConstructor) {
    BytesMut b;
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(b.len(), 0u);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesMutTest, WithCapacity) {
    auto b = BytesMut::with_capacity(100);
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(b.remaining_mut(), 100u);
}

TEST(BytesMutTest, CopyConstructor) {
    auto b1 = BytesMut::with_capacity(10);
    b1.put_u32_be(0xDEADBEEF);
    BytesMut b2(b1);
    EXPECT_EQ(b2.get_u32_be().unwrap(), 0xDEADBEEFu);
    EXPECT_EQ(b1.remaining(), 4u); // original is independent, position unchanged
}

TEST(BytesMutTest, MoveConstructor) {
    auto b1 = BytesMut::with_capacity(10);
    b1.put_u32_be(0xCAFEBABE);
    BytesMut b2(std::move(b1));
    EXPECT_EQ(b2.get_u32_be().unwrap(), 0xCAFEBABEu);
}

// ==================== BytesMut — 写 ====================

TEST(BytesMutTest, PutU8) {
    auto b = BytesMut::with_capacity(4);
    b.put_u8(0xAB);
    EXPECT_EQ(b.len(), 1u);
    EXPECT_EQ(b.get_u8().unwrap(), 0xAB);
}

TEST(BytesMutTest, PutU16_BigEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u16_be(0x0102);
    EXPECT_EQ(b.len(), 2u);
    EXPECT_EQ(b.get_u16_be().unwrap(), 0x0102u);
}

TEST(BytesMutTest, PutU16_LittleEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u16_le(0x0102);
    EXPECT_EQ(b.len(), 2u);
    // Re-read for verification via raw bytes
    EXPECT_EQ(b.as_ptr()[0], 0x02);
    EXPECT_EQ(b.as_ptr()[1], 0x01);
}

TEST(BytesMutTest, PutU32_BigEndian) {
    auto b = BytesMut::with_capacity(8);
    b.put_u32_be(0xDEADBEEF);
    b.put_u32_be(0xCAFEBABE);
    EXPECT_EQ(b.len(), 8u);
    EXPECT_EQ(b.get_u32_be().unwrap(), 0xDEADBEEFu);
    EXPECT_EQ(b.get_u32_be().unwrap(), 0xCAFEBABEu);
}

TEST(BytesMutTest, PutU32_LittleEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u32_le(0x01020304);
    // verify by reading back in BE to see the byte reversal
    EXPECT_EQ(b.get_u32_be().unwrap(), 0x04030201u);
}

TEST(BytesMutTest, PutU64) {
    auto b = BytesMut::with_capacity(16);
    b.put_u64_be(0x0102030405060708u);
    b.put_u64_le(0x0102030405060708u);
    EXPECT_EQ(b.get_u64_be().unwrap(), 0x0102030405060708u);
    EXPECT_EQ(b.get_u64_be().unwrap(), 0x0807060504030201u);
}

TEST(BytesMutTest, PutF32) {
    auto b = BytesMut::with_capacity(4);
    b.put_f32_be(3.14159f);
    EXPECT_NEAR(b.get_f32_be().unwrap(), 3.14159f, 1e-5f);
}

TEST(BytesMutTest, PutF64) {
    auto b = BytesMut::with_capacity(8);
    b.put_f64_be(3.14159265358979);
    EXPECT_NEAR(b.get_f64_be().unwrap(), 3.14159265358979, 1e-14);
}

TEST(BytesMutTest, PutSlice) {
    auto b = BytesMut::with_capacity(10);
    u8 data[] = {0x01, 0x02, 0x03};
    b.put_slice(data, 3);
    EXPECT_EQ(b.len(), 3u);
    EXPECT_EQ(b.get_u8().unwrap(), 0x01);
    EXPECT_EQ(b.get_u8().unwrap(), 0x02);
    EXPECT_EQ(b.get_u8().unwrap(), 0x03);
}

// ==================== BytesMut — 自动扩容 ====================

TEST(BytesMutTest, AutoGrow) {
    auto b = BytesMut::with_capacity(2);
    b.put_u8(0x01);
    b.put_u8(0x02);
    b.put_u8(0x03); // forces grow
    EXPECT_EQ(b.len(), 3u);
    EXPECT_EQ(b.get_u8().unwrap(), 0x01);
    EXPECT_EQ(b.get_u8().unwrap(), 0x02);
    EXPECT_EQ(b.get_u8().unwrap(), 0x03);
}

TEST(BytesMutTest, Reserve) {
    auto b = BytesMut::with_capacity(4);
    b.reserve(100);
    EXPECT_GE(b.remaining_mut(), 100u);
}

// ==================== BytesMut — truncate / clear ====================

TEST(BytesMutTest, Clear) {
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0xDEADBEEF);
    b.clear();
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(b.len(), 0u);
    EXPECT_EQ(b.remaining(), 0u);
    // should be able to write again
    b.put_u32_be(0xCAFEBABE);
    EXPECT_EQ(b.get_u32_be().unwrap(), 0xCAFEBABEu);
}

TEST(BytesMutTest, Truncate) {
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0x01020304);
    b.truncate(2);
    EXPECT_EQ(b.len(), 2u);
}

// ==================== BytesMut — 读 ====================

TEST(BytesMutTest, ReadAfterWrite) {
    auto b = BytesMut::with_capacity(10);
    b.put_u8(0x01);
    b.put_u16_be(0x0203);
    b.put_u32_be(0x04050607);
    EXPECT_EQ(b.get_u8().unwrap(), 0x01);
    EXPECT_EQ(b.get_u16_be().unwrap(), 0x0203u);
    EXPECT_EQ(b.get_u32_be().unwrap(), 0x04050607u);
}

TEST(BytesMutTest, Advance) {
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0x01020304);
    EXPECT_TRUE(b.advance(2).is_ok()); // skip first 2 bytes
    EXPECT_EQ(b.get_u16_be().unwrap(), 0x0304u);
}

TEST(BytesMutTest, ReadUnderflowReturnsErr) {
    BytesMut b;
    EXPECT_TRUE(b.get_u8().is_err());
}

// ==================== BytesMut — 冻结 ====================

TEST(BytesMutTest, Freeze) {
    auto mut = BytesMut::with_capacity(10);
    mut.put_u32_be(0xDEADBEEF);
    auto frozen = mut.freeze();
    EXPECT_TRUE(mut.is_empty());
    EXPECT_EQ(frozen.len(), 4u);
    EXPECT_EQ(frozen.get_u32_be().unwrap(), 0xDEADBEEFu);
}

TEST(BytesMutTest, FreezeEmpty) {
    auto mut = BytesMut::with_capacity(10);
    auto frozen = mut.freeze();
    EXPECT_TRUE(frozen.is_empty());
}

// ==================== BytesMut — 比较 ====================

TEST(BytesMutTest, Equals) {
    auto a = BytesMut::with_capacity(10);
    a.put_u32_be(0x01020304);
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0x01020304);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(BytesMutTest, NotEquals) {
    auto a = BytesMut::with_capacity(10);
    a.put_u32_be(0x01020304);
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0x05060708);
    EXPECT_TRUE(a != b);
}

// ==================== ByteSlice ====================

TEST(ByteSliceTest, DefaultConstructor) {
    ByteSlice s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
}

TEST(ByteSliceTest, FromData) {
    u8 data[] = {0x01, 0x02, 0x03};
    ByteSlice s(data, 3);
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.data(), static_cast<const u8*>(data));
}

TEST(ByteSliceTest, IndexAccess) {
    u8 data[] = {0x0A, 0x0B, 0x0C};
    ByteSlice s(data, 3);
    EXPECT_EQ(s[0], 0x0A);
    EXPECT_EQ(s[1], 0x0B);
    EXPECT_EQ(s[2], 0x0C);
}

TEST(ByteSliceTest, SubSlice) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ByteSlice s(data, 5);
    auto sub = s.sub_slice(1, 3);
    EXPECT_EQ(sub.size(), 3u);
    EXPECT_EQ(sub[0], 0x02);
    EXPECT_EQ(sub[1], 0x03);
    EXPECT_EQ(sub[2], 0x04);
}

TEST(ByteSliceTest, SubSliceFull) {
    u8 data[] = {0x01, 0x02, 0x03};
    ByteSlice s(data, 3);
    auto sub = s.sub_slice(0, 3);
    EXPECT_EQ(sub.size(), 3u);
    EXPECT_EQ(sub[0], 0x01);
    EXPECT_EQ(sub[2], 0x03);
}

TEST(ByteSliceTest, SubSliceEmpty) {
    u8 data[] = {0x01, 0x02, 0x03};
    ByteSlice s(data, 3);
    auto sub = s.sub_slice(1, 0);
    EXPECT_TRUE(sub.empty());
}

TEST(ByteSliceTest, SubSliceOutOfRange) {
    u8 data[] = {0x01, 0x02};
    ByteSlice s(data, 2);
    EXPECT_THROW(s.sub_slice(1, 2), std::out_of_range);
    EXPECT_THROW(s.sub_slice(3, 1), std::out_of_range);
}

// ==================== varint（LEB128）— put 精确字节序列 ====================
// 期望字节序列由 python enc(n)（逐字节 n&0x7F、右移 7）现算核对后硬编码。

namespace {

// 把 value 编码进新缓冲并逐字节核对期望序列。
void check_put_var_u32(u32 value, const u8* expect, usize len) {
    BytesMut b;
    b.put_var_u32(value);
    ASSERT_EQ(b.len(), len);
    for (usize i = 0; i < len; ++i) {
        EXPECT_EQ(b.as_ptr()[i], expect[i]);
    }
}

void check_put_var_u64(u64 value, const u8* expect, usize len) {
    BytesMut b;
    b.put_var_u64(value);
    ASSERT_EQ(b.len(), len);
    for (usize i = 0; i < len; ++i) {
        EXPECT_EQ(b.as_ptr()[i], expect[i]);
    }
}

} // namespace

TEST(BytesMutTest, PutVarU32_BoundaryEncodings) {
    static const u8 e0[]      = {0x00};
    static const u8 e127[]    = {0x7F};
    static const u8 e128[]    = {0x80, 0x01};
    static const u8 e150[]    = {0x96, 0x01};
    static const u8 e16383[]  = {0xFF, 0x7F};
    static const u8 e16384[]  = {0x80, 0x80, 0x01};
    static const u8 eumax[]   = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    check_put_var_u32(0, e0, 1);
    check_put_var_u32(127, e127, 1);
    check_put_var_u32(128, e128, 2);
    check_put_var_u32(150, e150, 2);
    check_put_var_u32(16383, e16383, 2);
    check_put_var_u32(16384, e16384, 3);
    check_put_var_u32(UINT32_MAX, eumax, 5);
}

TEST(BytesMutTest, PutVarU64_BoundaryEncodings) {
    static const u8 e32bit[]  = {0x80, 0x80, 0x80, 0x80, 0x10};             // 2^32
    static const u8 e63bit[]  = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}; // 2^63
    static const u8 eumax[]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}; // u64max
    check_put_var_u64(4294967296ull, e32bit, 5);
    check_put_var_u64(9223372036854775808ull, e63bit, 10);
    check_put_var_u64(UINT64_MAX, eumax, 10);
}

TEST(BytesMutTest, PutVarU32_AutoGrow) {
    auto b = BytesMut::with_capacity(2); // 容量不足，迫使 grow
    b.put_var_u32(UINT32_MAX);
    EXPECT_EQ(b.len(), 5u);
    EXPECT_EQ(b.get_var_u32().unwrap(), UINT32_MAX);
}

// ==================== varint — 合法读取（Bytes 与 BytesMut 两条通路） ====================

TEST(BytesTest, GetVarU32_ReadsAndAdvancesCursor) {
    u8 data[] = {0x96, 0x01, 0x80, 0x80, 0x01}; // 150、16384 连续排布
    auto b = Bytes::from_static(data, 5);
    EXPECT_EQ(b.get_var_u32().unwrap(), 150u);
    EXPECT_EQ(b.remaining(), 3u); // 前进 2 字节
    EXPECT_EQ(b.get_var_u32().unwrap(), 16384u);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, GetVarU32_BoundaryValues) {
    u8 data[] = {0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F}; // 0、127、u32max
    auto b = Bytes::from_static(data, 7);
    EXPECT_EQ(b.get_var_u32().unwrap(), 0u);
    EXPECT_EQ(b.get_var_u32().unwrap(), 127u);
    EXPECT_EQ(b.get_var_u32().unwrap(), UINT32_MAX);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, GetVarU64_BoundaryValues) {
    u8 data[] = {0x96, 0x01,
                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}; // 150、u64max
    auto b = Bytes::from_static(data, sizeof(data));
    EXPECT_EQ(b.get_var_u64().unwrap(), 150u);
    EXPECT_EQ(b.get_var_u64().unwrap(), UINT64_MAX);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesMutTest, GetVarU32_ReadsAndAdvancesCursor) {
    auto b = BytesMut::with_capacity(8);
    b.put_var_u32(0);
    b.put_var_u32(150);
    b.put_var_u32(UINT32_MAX);
    EXPECT_EQ(b.get_var_u32().unwrap(), 0u);
    EXPECT_EQ(b.get_var_u32().unwrap(), 150u);
    EXPECT_EQ(b.get_var_u32().unwrap(), UINT32_MAX);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesMutTest, GetVarU64_ReadsAndAdvancesCursor) {
    auto b = BytesMut::with_capacity(16);
    b.put_var_u64(9223372036854775808ull); // 2^63
    b.put_var_u64(16384);
    EXPECT_EQ(b.get_var_u64().unwrap(), 9223372036854775808ull);
    EXPECT_EQ(b.get_var_u64().unwrap(), 16384u);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, VarRoundTrip_BoundaryAndZigzagNegatives) {
    BytesMut src;
    src.put_var_u32(0);
    src.put_var_u32(UINT32_MAX);
    src.put_var_u64(UINT64_MAX);
    // zigzag 后的负数 round-trip：-1、i32/i64 两端的极值
    const i32 i32_cases[] = {-1, INT32_MIN, INT32_MAX};
    for (i32 v : i32_cases) {
        src.put_var_u32(zigzag_encode32(v));
    }
    const i64 i64_cases[] = {-1, INT64_MIN, INT64_MAX};
    for (i64 v : i64_cases) {
        src.put_var_u64(zigzag_encode64(v));
    }
    Bytes b = src.freeze();
    EXPECT_EQ(b.get_var_u32().unwrap(), 0u);
    EXPECT_EQ(b.get_var_u32().unwrap(), UINT32_MAX);
    EXPECT_EQ(b.get_var_u64().unwrap(), UINT64_MAX);
    EXPECT_EQ(zigzag_decode32(b.get_var_u32().unwrap()), -1);
    EXPECT_EQ(zigzag_decode32(b.get_var_u32().unwrap()), INT32_MIN);
    EXPECT_EQ(zigzag_decode32(b.get_var_u32().unwrap()), INT32_MAX);
    EXPECT_EQ(zigzag_decode64(b.get_var_u64().unwrap()), -1);
    EXPECT_EQ(zigzag_decode64(b.get_var_u64().unwrap()), INT64_MIN);
    EXPECT_EQ(zigzag_decode64(b.get_var_u64().unwrap()), INT64_MAX);
    EXPECT_EQ(b.remaining(), 0u);
}

// ==================== varint — 截断 → Underflow 且游标不动 ====================

TEST(BytesTest, GetVarU32_UnderflowKeepsCursor) {
    u8 data[] = {0x7F, 0x80, 0x80, 0x80, 0x80}; // 127 正常，后 4 字节续位无终止
    auto b = Bytes::from_static(data, 5);
    EXPECT_EQ(b.get_var_u32().unwrap(), 127u);
    EXPECT_EQ(b.remaining(), 4u);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::Underflow);
    EXPECT_EQ(b.remaining(), 4u); // 游标不动
}

TEST(BytesTest, GetVarU32_UnderflowOnEmpty) {
    auto b = Bytes::from_static(nullptr, 0);
    EXPECT_TRUE(b.get_var_u32().is_err());
    EXPECT_TRUE(b.get_var_u64().is_err());
}

TEST(BytesTest, GetVarU64_UnderflowKeepsCursor) {
    u8 data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}; // 9 字节续位，缺第 10 字节
    auto b = Bytes::from_static(data, 9);
    EXPECT_EQ(b.remaining(), 9u);
    auto r = b.get_var_u64();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::Underflow);
    EXPECT_EQ(b.remaining(), 9u); // 游标不动
}

TEST(BytesMutTest, GetVarU32_UnderflowKeepsCursor) {
    BytesMut b;
    b.put_u8(0x80); // 单个续位字节
    EXPECT_EQ(b.remaining(), 1u);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::Underflow);
    EXPECT_EQ(b.remaining(), 1u); // 游标不动
}

// ==================== varint — 非规范编码 → MalformedVarint 且游标不动 ====================

TEST(BytesTest, GetVarU32_MalformedRedundantPadding) {
    u8 data[] = {0x80, 0x00}; // 末字节 0x00 冗余填充（0 只该占 1 字节）
    auto b = Bytes::from_static(data, 2);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 2u); // 游标不动
}

TEST(BytesTest, GetVarU64_MalformedRedundantPadding) {
    u8 data[] = {0xFF, 0x00}; // 终止字节 0x00：等价于 [7F]
    auto b = Bytes::from_static(data, 2);
    auto r = b.get_var_u64();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 2u); // 游标不动
}

TEST(BytesTest, GetVarU32_MalformedTooLong) {
    u8 data[] = {0x80, 0x80, 0x80, 0x80, 0x80}; // 5 字节仍带续位
    auto b = Bytes::from_static(data, 5);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 5u); // 游标不动
}

TEST(BytesTest, GetVarU32_MalformedFifthByteOutOfRange) {
    u8 data[] = {0x80, 0x80, 0x80, 0x80, 0x10}; // 第 5 字节 0x10 > 0x0F，高位越界
    auto b = Bytes::from_static(data, 5);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 5u); // 游标不动
}

TEST(BytesTest, GetVarU64_MalformedTooLong) {
    u8 data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}; // 11 字节
    auto b = Bytes::from_static(data, 11);
    auto r = b.get_var_u64();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 11u); // 游标不动
}

TEST(BytesTest, GetVarU64_MalformedTenthByteOutOfRange) {
    u8 data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}; // 第 10 字节只允许 0/1
    auto b = Bytes::from_static(data, 10);
    auto r = b.get_var_u64();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 10u); // 游标不动
}

TEST(BytesMutTest, GetVarU32_MalformedKeepsCursor) {
    BytesMut b;
    const u8 bytes[] = {0x80, 0x00};
    b.put_slice(bytes, 2);
    auto r = b.get_var_u32();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 2u); // 游标不动
}

TEST(BytesMutTest, GetVarU64_MalformedTooLongKeepsCursor) {
    BytesMut b;
    for (int i = 0; i < 11; ++i) {
        b.put_u8(0x80);
    }
    auto r = b.get_var_u64();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.unwrap_err(), BytesError::MalformedVarint);
    EXPECT_EQ(b.remaining(), 11u); // 游标不动
}

// ==================== varint — 确定性伪随机 round-trip ====================

TEST(BytesTest, VarRoundTrip_DeterministicPseudoRandom) {
    // 固定种子 LCG（Numerical Recipes 参数），不引入随机模块。
    u32 seed = 20240913u;
    auto next_u32 = [&seed]() -> u32 {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    auto next_u64 = [&next_u32]() -> u64 {
        const u64 hi = next_u32();
        const u64 lo = next_u32();
        return (hi << 32) | lo;
    };

    BytesMut src;
    for (int i = 0; i < 1000; ++i) {
        src.put_var_u32(next_u32());
        src.put_var_u64(next_u64());
        src.put_var_u32(zigzag_encode32(static_cast<i32>(next_u32())));
        src.put_var_u64(zigzag_encode64(static_cast<i64>(next_u64())));
    }
    ASSERT_FALSE(src.is_empty());
    Bytes b = src.freeze();

    seed = 20240913u; // 重置种子，按相同顺序重现期望值
    for (int i = 0; i < 1000; ++i) {
        const u32 v32 = next_u32();
        const u64 v64 = next_u64();
        const i32 vi32 = static_cast<i32>(next_u32());
        const i64 vi64 = static_cast<i64>(next_u64());

        auto r1 = b.get_var_u32();
        ASSERT_TRUE(r1.is_ok());
        EXPECT_EQ(r1.unwrap(), v32);
        auto r2 = b.get_var_u64();
        ASSERT_TRUE(r2.is_ok());
        EXPECT_EQ(r2.unwrap(), v64);
        auto r3 = b.get_var_u32();
        ASSERT_TRUE(r3.is_ok());
        EXPECT_EQ(zigzag_decode32(r3.unwrap()), vi32);
        auto r4 = b.get_var_u64();
        ASSERT_TRUE(r4.is_ok());
        EXPECT_EQ(zigzag_decode64(r4.unwrap()), vi64);
    }
    EXPECT_EQ(b.remaining(), 0u); // 恰好读完
}

// ==================== zigzag ====================

TEST(ZigzagTest, Encode32_BoundaryValues) {
    EXPECT_EQ(zigzag_encode32(0), 0u);
    EXPECT_EQ(zigzag_encode32(-1), 1u);
    EXPECT_EQ(zigzag_encode32(1), 2u);
    EXPECT_EQ(zigzag_encode32(-2), 3u);
    EXPECT_EQ(zigzag_encode32(-100), 199u);        // python 核对
    EXPECT_EQ(zigzag_encode32(INT32_MIN), UINT32_MAX);
    EXPECT_EQ(zigzag_encode32(INT32_MAX), 4294967294u);
}

TEST(ZigzagTest, Decode32_BoundaryValues) {
    EXPECT_EQ(zigzag_decode32(0u), 0);
    EXPECT_EQ(zigzag_decode32(1u), -1);
    EXPECT_EQ(zigzag_decode32(2u), 1);
    EXPECT_EQ(zigzag_decode32(3u), -2);
    EXPECT_EQ(zigzag_decode32(UINT32_MAX), INT32_MIN);
    EXPECT_EQ(zigzag_decode32(4294967294u), INT32_MAX);
}

TEST(ZigzagTest, Encode64_BoundaryValues) {
    EXPECT_EQ(zigzag_encode64(0), 0u);
    EXPECT_EQ(zigzag_encode64(-1), 1u);
    EXPECT_EQ(zigzag_encode64(INT64_MIN), UINT64_MAX);
    EXPECT_EQ(zigzag_encode64(INT64_MAX), 18446744073709551614ull);
}

TEST(ZigzagTest, Decode64_BoundaryValues) {
    EXPECT_EQ(zigzag_decode64(0u), 0);
    EXPECT_EQ(zigzag_decode64(1u), -1);
    EXPECT_EQ(zigzag_decode64(UINT64_MAX), INT64_MIN);
    EXPECT_EQ(zigzag_decode64(18446744073709551614ull), INT64_MAX);
}

TEST(ZigzagTest, RoundTrip32) {
    const i32 cases[] = {0, 1, -1, 2, -2, 63, -64, 64, -65, 123456789, -123456789, INT32_MIN, INT32_MAX};
    for (i32 v : cases) {
        EXPECT_EQ(zigzag_decode32(zigzag_encode32(v)), v);
    }
}

TEST(ZigzagTest, RoundTrip64) {
    const i64 cases[] = {0, 1, -1, 2, -2, 63, -64, 64, -65,
                         5000000000LL, -5000000000LL, INT64_MIN, INT64_MAX};
    for (i64 v : cases) {
        EXPECT_EQ(zigzag_decode64(zigzag_encode64(v)), v);
    }
}

// ==================== fill：已写区域整体填充（敏感缓冲清零入口） ====================

TEST(BytesMutTest, FillOverwritesAllWrittenBytes) {
    auto b = BytesMut::with_capacity(16);
    const u8 secret[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37};
    b.put_slice(secret, 6);

    b.fill(0);
    EXPECT_EQ(b.len(), 6u);  // 长度与游标不受 fill 影响
    for (usize i = 0; i < 6; ++i) {
        EXPECT_EQ(b.get_u8().unwrap(), 0u);
    }

    // fill 后缓冲仍可继续写入
    b.put_u8(0xAB);
    EXPECT_EQ(b.get_u8().unwrap(), 0xAB);
}

TEST(BytesMutTest, FillCoversBytesBeforeReadCursor) {
    auto b = BytesMut::with_capacity(8);
    const u8 secret[] = {0x01, 0x02, 0x03, 0x04};
    b.put_slice(secret, 4);
    ASSERT_TRUE(b.advance(2).is_ok());  // 前 2 字节已读：fill 同样要清掉

    b.fill(0);
    EXPECT_EQ(b.remaining(), 2u);
    for (usize i = 0; i < 2; ++i) {
        EXPECT_EQ(b.get_u8().unwrap(), 0u);
    }
}

TEST(BytesMutTest, FillEmptyIsNoop) {
    BytesMut b;
    b.fill(0xFF);  // 未分配缓冲不崩溃
    EXPECT_EQ(b.len(), 0u);
}

}} // namespace ca::core::test
