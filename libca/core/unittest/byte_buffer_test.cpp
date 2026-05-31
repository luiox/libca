#include <gtest/gtest.h>
#include <libca/core/byte_buffer.hpp>

namespace ca::core {

TEST(ByteBufferTest, DefaultConstructor) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
}

TEST(ByteBufferTest, FromData) {
    u8 data[] = {0x10, 0x20, 0x30};
    ByteBuffer buf(data, 3);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[2], 0x30);
}

TEST(ByteBufferTest, CopyMove) {
    ByteBuffer b1;
    b1.pushBack(0x41);
    ByteBuffer b2(b1);
    EXPECT_EQ(b2.size(), 1);
    EXPECT_NE(b1.data(), b2.data());
    ByteBuffer b3(std::move(b1));
    EXPECT_EQ(b3.size(), 1);
}

TEST(ByteBufferTest, PushPopBack) {
    ByteBuffer buf;
    buf.pushBack(0x10);
    buf.pushBack(0x20);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf.back(), 0x20);
    buf.popBack();
    EXPECT_EQ(buf.size(), 1);
    EXPECT_EQ(buf.front(), 0x10);
}

TEST(ByteBufferTest, AppendAndInsert) {
    ByteBuffer buf;
    buf.pushBack(0x10);
    buf.pushBack(0x30);
    u8 mid[] = {0x20};
    buf.insert(1, mid, 1);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[1], 0x20);
    EXPECT_EQ(buf[2], 0x30);
}

TEST(ByteBufferTest, Erase) {
    ByteBuffer buf;
    for (int i = 0; i < 5; ++i) buf.pushBack(static_cast<u8>(i));
    buf.erase(1, 3);
    EXPECT_EQ(buf.size(), 2);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[1], 4);
}

TEST(ByteBufferTest, Equals) {
    u8 a[] = {0x01, 0x02};
    u8 b[] = {0x01, 0x02};
    u8 c[] = {0x01, 0x03};
    EXPECT_TRUE(ByteBuffer(a, 2) == ByteBuffer(b, 2));
    EXPECT_TRUE(ByteBuffer(a, 2) != ByteBuffer(c, 2));
}

TEST(ByteBufferTest, ClearReserveShrink) {
    ByteBuffer buf;
    buf.pushBack(0x41);
    buf.reserve(256);
    EXPECT_GE(buf.capacity(), 256);
    buf.shrinkToFit();
    EXPECT_EQ(buf.size(), 1);
    buf.clear();
    EXPECT_TRUE(buf.empty());
}

TEST(ByteBufferTest, AtException) {
    ByteBuffer buf;
    EXPECT_THROW(buf.at(0), std::out_of_range);
}

TEST(ByteBufferTest, AssignSwap) {
    ByteBuffer buf;
    buf.pushBack(0xFF);
    u8 data[] = {0x10, 0x20};
    buf.assign(data, 2);
    EXPECT_EQ(buf.size(), 2);

    ByteBuffer other;
    other.pushBack(0x99);
    buf.swap(other);
    EXPECT_EQ(buf[0], 0x99);
    EXPECT_EQ(other[0], 0x10);
}

TEST(ByteBufferTest, LargePushBack) {
    ByteBuffer buf;
    for (int i = 0; i < 100000; ++i)
        buf.pushBack(static_cast<u8>(i & 0xFF));
    EXPECT_EQ(buf.size(), 100000);
    EXPECT_EQ(buf[99999], static_cast<u8>(99999 & 0xFF));
}

}  // namespace ca::core
