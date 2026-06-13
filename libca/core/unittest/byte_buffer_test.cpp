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
    EXPECT_EQ(b.get_u8(), 0x01);
    EXPECT_EQ(b.remaining(), 3u);
    b.advance(2);
    EXPECT_EQ(b.remaining(), 1u);
    EXPECT_EQ(b.get_u8(), 0x04);
    EXPECT_EQ(b.remaining(), 0u);
}

TEST(BytesTest, AdvancePastEndThrows) {
    u8 data[] = {0x01};
    auto b = Bytes::from_static(data, 1);
    EXPECT_THROW(b.advance(2), std::out_of_range);
}

TEST(BytesTest, ReadPastEndThrows) {
    auto b = Bytes::from_static(nullptr, 0);
    EXPECT_THROW(b.get_u8(), std::out_of_range);
}

// ==================== Bytes — 切片 ====================

TEST(BytesTest, Slice) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto b = Bytes::from_static(data, 5);
    auto s = b.slice(1, 4);
    EXPECT_EQ(s.len(), 3u);
    EXPECT_EQ(s.get_u8(), 0x02);
    EXPECT_EQ(s.get_u8(), 0x03);
    EXPECT_EQ(s.get_u8(), 0x04);
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
    EXPECT_EQ(s1.get_u8(), 'h');
    EXPECT_EQ(s2.get_u8(), 'w');
}

// ==================== Bytes — 类型化读 ====================

TEST(BytesTest, GetU16_BigEndian) {
    u8 data[] = {0xAB, 0xCD};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_u16_be(), 0xABCDu);
}

TEST(BytesTest, GetU16_LittleEndian) {
    u8 data[] = {0xCD, 0xAB};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_u16_le(), 0xABCDu);
}

TEST(BytesTest, GetU32_BigEndian) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04};
    auto b = Bytes::from_static(data, 4);
    EXPECT_EQ(b.get_u32_be(), 0x01020304u);
}

TEST(BytesTest, GetU32_LittleEndian) {
    u8 data[] = {0x04, 0x03, 0x02, 0x01};
    auto b = Bytes::from_static(data, 4);
    EXPECT_EQ(b.get_u32_le(), 0x01020304u);
}

TEST(BytesTest, GetU64_BigEndian) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto b = Bytes::from_static(data, 8);
    EXPECT_EQ(b.get_u64_be(), 0x0102030405060708u);
}

TEST(BytesTest, GetU64_LittleEndian) {
    u8 data[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    auto b = Bytes::from_static(data, 8);
    EXPECT_EQ(b.get_u64_le(), 0x0102030405060708u);
}

TEST(BytesTest, GetI16) {
    u8 data[] = {0xFF, 0xF6};
    auto b = Bytes::from_static(data, 2);
    EXPECT_EQ(b.get_i16_be(), -10);
}

TEST(BytesTest, GetF32) {
    u8 data[] = {0x40, 0x49, 0x0F, 0xDB}; // 3.14159f in big-endian IEEE 754
    auto b = Bytes::from_static(data, 4);
    EXPECT_NEAR(b.get_f32_be(), 3.14159f, 1e-5f);
}

TEST(BytesTest, GetValuesSequentially) {
    u8 data[] = {0x01,           // u8  = 0x01
                 0x02, 0x03,     // u16 = 0x0203
                 0x04, 0x05, 0x06, 0x07, // u32 = 0x04050607
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // u64
    auto b = Bytes::from_static(data, 16);
    EXPECT_EQ(b.get_u8(),    0x01u);
    EXPECT_EQ(b.get_u16_be(),   0x0203u);
    EXPECT_EQ(b.get_u32_be(),   0x04050607u);
    EXPECT_EQ(b.get_u64_be(),   0x08090A0B0C0D0E0Fu);
}

// ==================== Bytes — 批量读 ====================

TEST(BytesTest, CopyToSlice) {
    u8 data[] = {0x01, 0x02, 0x03, 0x04};
    auto b = Bytes::from_static(data, 4);
    u8 dst[3];
    b.copy_to_slice(dst, 3);
    EXPECT_EQ(dst[0], 0x01);
    EXPECT_EQ(dst[1], 0x02);
    EXPECT_EQ(dst[2], 0x03);
    EXPECT_EQ(b.remaining(), 1u); // advanced
}

TEST(BytesTest, CopyToSliceUnderflow) {
    u8 data[] = {0x01};
    auto b = Bytes::from_static(data, 1);
    u8 dst[3];
    EXPECT_THROW(b.copy_to_slice(dst, 3), std::out_of_range);
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
    EXPECT_EQ(b2.get_u32_be(), 0xDEADBEEFu);
    EXPECT_EQ(b1.remaining(), 4u); // original is independent, position unchanged
}

TEST(BytesMutTest, MoveConstructor) {
    auto b1 = BytesMut::with_capacity(10);
    b1.put_u32_be(0xCAFEBABE);
    BytesMut b2(std::move(b1));
    EXPECT_EQ(b2.get_u32_be(), 0xCAFEBABEu);
}

// ==================== BytesMut — 写 ====================

TEST(BytesMutTest, PutU8) {
    auto b = BytesMut::with_capacity(4);
    b.put_u8(0xAB);
    EXPECT_EQ(b.len(), 1u);
    EXPECT_EQ(b.get_u8(), 0xAB);
}

TEST(BytesMutTest, PutU16_BigEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u16_be(0x0102);
    EXPECT_EQ(b.len(), 2u);
    EXPECT_EQ(b.get_u16_be(), 0x0102u);
}

TEST(BytesMutTest, PutU16_LittleEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u16_le(0x0102);
    b.advance(0); // reset position for read
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
    EXPECT_EQ(b.get_u32_be(), 0xDEADBEEFu);
    EXPECT_EQ(b.get_u32_be(), 0xCAFEBABEu);
}

TEST(BytesMutTest, PutU32_LittleEndian) {
    auto b = BytesMut::with_capacity(4);
    b.put_u32_le(0x01020304);
    // verify by reading back in BE to see the byte reversal
    EXPECT_EQ(b.get_u32_be(), 0x04030201u);
}

TEST(BytesMutTest, PutU64) {
    auto b = BytesMut::with_capacity(16);
    b.put_u64_be(0x0102030405060708u);
    b.put_u64_le(0x0102030405060708u);
    b.advance(0);
    EXPECT_EQ(b.get_u64_be(), 0x0102030405060708u);
    EXPECT_EQ(b.get_u64_be(), 0x0807060504030201u);
}

TEST(BytesMutTest, PutF32) {
    auto b = BytesMut::with_capacity(4);
    b.put_f32_be(3.14159f);
    b.advance(0);
    EXPECT_NEAR(b.get_f32_be(), 3.14159f, 1e-5f);
}

TEST(BytesMutTest, PutF64) {
    auto b = BytesMut::with_capacity(8);
    b.put_f64_be(3.14159265358979);
    b.advance(0);
    EXPECT_NEAR(b.get_f64_be(), 3.14159265358979, 1e-14);
}

TEST(BytesMutTest, PutSlice) {
    auto b = BytesMut::with_capacity(10);
    u8 data[] = {0x01, 0x02, 0x03};
    b.put_slice(data, 3);
    EXPECT_EQ(b.len(), 3u);
    EXPECT_EQ(b.get_u8(), 0x01);
    EXPECT_EQ(b.get_u8(), 0x02);
    EXPECT_EQ(b.get_u8(), 0x03);
}

// ==================== BytesMut — 自动扩容 ====================

TEST(BytesMutTest, AutoGrow) {
    auto b = BytesMut::with_capacity(2);
    b.put_u8(0x01);
    b.put_u8(0x02);
    b.put_u8(0x03); // forces grow
    EXPECT_EQ(b.len(), 3u);
    EXPECT_EQ(b.get_u8(), 0x01);
    EXPECT_EQ(b.get_u8(), 0x02);
    EXPECT_EQ(b.get_u8(), 0x03);
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
    EXPECT_EQ(b.get_u32_be(), 0xCAFEBABEu);
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
    EXPECT_EQ(b.get_u8(), 0x01);
    EXPECT_EQ(b.get_u16_be(), 0x0203u);
    EXPECT_EQ(b.get_u32_be(), 0x04050607u);
}

TEST(BytesMutTest, Advance) {
    auto b = BytesMut::with_capacity(10);
    b.put_u32_be(0x01020304);
    b.advance(2); // skip first 2 bytes
    EXPECT_EQ(b.get_u16_be(), 0x0304u);
}

TEST(BytesMutTest, ReadUnderflowThrows) {
    BytesMut b;
    EXPECT_THROW(b.get_u8(), std::out_of_range);
}

// ==================== BytesMut — 冻结 ====================

TEST(BytesMutTest, Freeze) {
    auto mut = BytesMut::with_capacity(10);
    mut.put_u32_be(0xDEADBEEF);
    auto frozen = mut.freeze();
    EXPECT_TRUE(mut.is_empty());
    EXPECT_EQ(frozen.len(), 4u);
    EXPECT_EQ(frozen.get_u32_be(), 0xDEADBEEFu);
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

}} // namespace ca::core::test
