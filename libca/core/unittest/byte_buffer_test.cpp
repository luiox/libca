#include <gtest/gtest.h>
#include <libca/core/byte_buffer.hpp>

namespace ca::core {

// ============================================================================
// 工厂方法
// ============================================================================

TEST(ByteBufferTest, Allocate) {
    auto buf = ByteBuffer::allocate(100);
    EXPECT_EQ(buf.capacity(), 100);
    EXPECT_EQ(buf.position(), 0);
    EXPECT_EQ(buf.limit(), 100);
    EXPECT_TRUE(buf.hasRemaining());
    EXPECT_EQ(buf.remaining(), 100);
    EXPECT_FALSE(buf.empty());
}

TEST(ByteBufferTest, AllocateZero) {
    auto buf = ByteBuffer::allocate(0);
    EXPECT_EQ(buf.capacity(), 1);   // minimum 1
    EXPECT_TRUE(buf.empty());       // position(0) == limit(0) → remaining 0
}

TEST(ByteBufferTest, CopyOf) {
    u8 data[] = {0x10, 0x20, 0x30};
    auto buf = ByteBuffer::copyOf(data, 3);
    EXPECT_EQ(buf.capacity(), 3);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[2], 0x30);
}

TEST(ByteBufferTest, WrapExternal) {
    u8 storage[] = {0x01, 0x02, 0x03};
    auto buf = ByteBuffer::wrap(storage, 3);
    EXPECT_EQ(buf.capacity(), 3);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[1], 0x02);

    // 修改外部数据，wrap 应看到变化
    storage[1] = 0xFF;
    EXPECT_EQ(buf[1], 0xFF);
}

// ============================================================================
// 兼容工厂别名
// ============================================================================

TEST(ByteBufferTest, FromDataAlias) {
    u8 data[] = {0x10, 0x20, 0x30};
    auto buf = ByteBuffer::fromData(data, 3);
    EXPECT_EQ(buf.size(), 3);
}

TEST(ByteBufferTest, WithCapacityAlias) {
    auto buf = ByteBuffer::withCapacity(64);
    EXPECT_EQ(buf.capacity(), 64);
}

// ============================================================================
// 构造 / 复制 / 移动
// ============================================================================

TEST(ByteBufferTest, DefaultConstructor) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.capacity(), 0);
    EXPECT_THROW(buf.position(1), std::out_of_range);
}

TEST(ByteBufferTest, DataConstructor) {
    u8 data[] = {0x10, 0x20, 0x30};
    ByteBuffer buf(data, 3);
    EXPECT_EQ(buf.size(), 3);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[2], 0x30);
}

TEST(ByteBufferTest, CopyConstructor) {
    ByteBuffer b1;
    b1.put(0x41);
    b1.put(0x42);
    b1.flip();
    EXPECT_EQ(b1.remaining(), 2);

    ByteBuffer b2(b1);
    EXPECT_EQ(b2.remaining(), 2);
    EXPECT_EQ(b2.get(), 0x41);
    EXPECT_NE(b1.data(), b2.data());
}

TEST(ByteBufferTest, MoveConstructor) {
    ByteBuffer b1;
    b1.put(0x41);
    b1.flip();

    const u8* oldPtr = b1.data();
    usize oldSize = b1.size();

    ByteBuffer b2(std::move(b1));
    EXPECT_EQ(b2.size(), oldSize);
    EXPECT_EQ(b2[0], 0x41);
    EXPECT_EQ(b2.data(), oldPtr);   // 移动后 b2 拥有原内存
    EXPECT_TRUE(b1.empty());
}

TEST(ByteBufferTest, CopyAssignment) {
    ByteBuffer b1;
    b1.put(0x41);
    b1.put(0x42);
    b1.flip();

    ByteBuffer b2;
    b2 = b1;
    EXPECT_EQ(b2.remaining(), 2);
    EXPECT_EQ(b2.get(), 0x41);
}

TEST(ByteBufferTest, MoveAssignment) {
    ByteBuffer b1;
    b1.put(0x41);
    b1.flip();

    const u8* oldPtr = b1.data();

    ByteBuffer b2;
    b2 = std::move(b1);
    EXPECT_EQ(b2[0], 0x41);
    EXPECT_EQ(b2.data(), oldPtr);
    EXPECT_TRUE(b1.empty());
}

// ============================================================================
// 游标控制
// ============================================================================

TEST(ByteBufferTest, PositionLimitConstraints) {
    auto buf = ByteBuffer::allocate(100);
    EXPECT_EQ(buf.position(), 0);
    EXPECT_EQ(buf.limit(), 100);

    buf.position(50);
    EXPECT_EQ(buf.position(), 50);

    buf.limit(80);
    EXPECT_EQ(buf.limit(), 80);
    EXPECT_EQ(buf.remaining(), 30);

    // 设置 limit 小于 position 时，position 自动回拉
    buf.limit(30);
    EXPECT_EQ(buf.position(), 30);
    EXPECT_EQ(buf.remaining(), 0);
}

TEST(ByteBufferTest, PositionOutOfRange) {
    auto buf = ByteBuffer::allocate(10);
    EXPECT_THROW(buf.position(11), std::out_of_range);
}

TEST(ByteBufferTest, LimitOutOfRange) {
    auto buf = ByteBuffer::allocate(10);
    EXPECT_THROW(buf.limit(11), std::out_of_range);
}

// ============================================================================
// Flip / Rewind / Clear
// ============================================================================

TEST(ByteBufferTest, FlipLifecycle) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.put(0x30);
    EXPECT_EQ(buf.position(), 3);
    EXPECT_EQ(buf.limit(), 10);

    buf.flip();
    EXPECT_EQ(buf.position(), 0);
    EXPECT_EQ(buf.limit(), 3);

    EXPECT_EQ(buf.get(), 0x10);
    EXPECT_EQ(buf.get(), 0x20);
    EXPECT_EQ(buf.get(), 0x30);
    EXPECT_FALSE(buf.hasRemaining());
}

TEST(ByteBufferTest, Rewind) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.flip();

    EXPECT_EQ(buf.get(), 0x10);
    EXPECT_EQ(buf.get(), 0x20);
    EXPECT_FALSE(buf.hasRemaining());

    buf.rewind();
    EXPECT_EQ(buf.position(), 0);
    EXPECT_EQ(buf.get(), 0x10);
}

TEST(ByteBufferTest, ClearSemantics) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.flip();
    buf.get();  // pos=1

    buf.clear();
    EXPECT_EQ(buf.position(), 0);
    EXPECT_EQ(buf.limit(), 10);   // Java clear: lim=cap
    EXPECT_EQ(buf.capacity(), 10);
    EXPECT_TRUE(buf.hasRemaining());
}

// ============================================================================
// Compact
// ============================================================================

TEST(ByteBufferTest, Compact) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x01);
    buf.put(0x02);
    buf.put(0x03);
    buf.put(0x04);
    buf.flip();

    buf.get();  // read 0x01 (pos=1)
    buf.get();  // read 0x02 (pos=2)

    buf.compact();
    // unread [0x03, 0x04] moved to front, position = 2
    EXPECT_EQ(buf.position(), 2);
    EXPECT_EQ(buf.limit(), 10);
    EXPECT_EQ(buf[0], 0x03);
    EXPECT_EQ(buf[1], 0x04);
}

// ============================================================================
// Mark / Reset
// ============================================================================

TEST(ByteBufferTest, MarkReset) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x01);
    buf.put(0x02);
    buf.flip();

    buf.get();          // pos=1
    buf.mark();
    buf.get();          // pos=2
    buf.reset();        // back to pos=1
    EXPECT_EQ(buf.position(), 1);
    EXPECT_EQ(buf.get(), 0x02);
}

TEST(ByteBufferTest, ResetWithoutMark) {
    auto buf = ByteBuffer::allocate(10);
    EXPECT_THROW(buf.reset(), std::runtime_error);
}

// ============================================================================
// 相对 get / put (单字节)
// ============================================================================

TEST(ByteBufferTest, PutAndGet) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.flip();

    EXPECT_EQ(buf.get(), 0x10);
    EXPECT_EQ(buf.get(), 0x20);
    EXPECT_FALSE(buf.hasRemaining());
}

TEST(ByteBufferTest, GetUnderflow) {
    auto buf = ByteBuffer::allocate(0);   // limit = 0, no readable bytes
    EXPECT_THROW(buf.get(), std::out_of_range);
}

// ============================================================================
// 相对批量 get / put
// ============================================================================

TEST(ByteBufferTest, BulkPutAndGet) {
    auto buf = ByteBuffer::allocate(10);
    u8 src[] = {0x10, 0x20, 0x30};
    buf.put(src, 3);
    buf.flip();

    u8 dst[3] = {};
    buf.get(dst, 3);
    EXPECT_EQ(dst[0], 0x10);
    EXPECT_EQ(dst[1], 0x20);
    EXPECT_EQ(dst[2], 0x30);
}

TEST(ByteBufferTest, BulkGetUnderflow) {
    auto buf = ByteBuffer::allocate(4);
    buf.put(0x01);
    buf.put(0x02);
    buf.flip();

    u8 dst[3] = {};
    EXPECT_THROW(buf.get(dst, 3), std::out_of_range);  // only 2 available
}

TEST(ByteBufferTest, PutFromAnotherBuffer) {
    auto src = ByteBuffer::allocate(10);
    src.put(0x10);
    src.put(0x20);
    src.flip();

    auto dst = ByteBuffer::allocate(10);
    dst.put(src);
    dst.flip();

    EXPECT_EQ(dst.get(), 0x10);
    EXPECT_EQ(dst.get(), 0x20);
}

// ============================================================================
// 绝对 get / put
// ============================================================================

TEST(ByteBufferTest, AbsolutePutAndGet) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(2, 0xFF);
    EXPECT_EQ(buf.get(2), 0xFF);
    // position 不受影响
    EXPECT_EQ(buf.position(), 0);
}

TEST(ByteBufferTest, AbsoluteBulk) {
    auto buf = ByteBuffer::allocate(10);
    u8 src[] = {0x01, 0x02, 0x03};
    buf.put(1, src, 3);

    u8 dst[3] = {};
    buf.get(1, dst, 3);
    EXPECT_EQ(dst[0], 0x01);
    EXPECT_EQ(dst[1], 0x02);
    EXPECT_EQ(dst[2], 0x03);
}

TEST(ByteBufferTest, AbsoluteOutOfRange) {
    auto buf = ByteBuffer::allocate(4);
    EXPECT_THROW(buf.get(4), std::out_of_range);
    EXPECT_THROW(buf.put(4, 0xFF), std::out_of_range);
}

// ============================================================================
// at() 边界检查
// ============================================================================

TEST(ByteBufferTest, AtException) {
    ByteBuffer buf;
    EXPECT_THROW(buf.at(0), std::out_of_range);
}

TEST(ByteBufferTest, AtValid) {
    u8 data[] = {0x10, 0x20};
    ByteBuffer buf(data, 2);
    EXPECT_EQ(buf.at(0), 0x10);
    EXPECT_EQ(buf.at(1), 0x20);
    EXPECT_THROW(buf.at(2), std::out_of_range);
}

// ============================================================================
// front / back
// ============================================================================

TEST(ByteBufferTest, FrontBack) {
    u8 data[] = {0x10, 0x20, 0x30};
    ByteBuffer buf(data, 3);
    EXPECT_EQ(buf.front(), 0x10);
    EXPECT_EQ(buf.back(), 0x30);
}

// ============================================================================
// 类型化读写 (无符号)
// ============================================================================

TEST(ByteBufferTest, PutGetU16_BigEndian) {
    auto buf = ByteBuffer::allocate(10);
    buf.putU16(0x0102);
    buf.flip();

    EXPECT_EQ(buf.getU16(), 0x0102);
    // 验证内存布局: BE → [0x01, 0x02]
    buf.rewind();
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
}

TEST(ByteBufferTest, PutGetU16_LittleEndian) {
    auto buf = ByteBuffer::allocate(10);
    buf.order(ByteOrder::LittleEndian);
    buf.putU16(0x0102);
    buf.flip();

    EXPECT_EQ(buf.getU16(), 0x0102);
    // 验证内存布局: LE → [0x02, 0x01]
    buf.rewind();
    EXPECT_EQ(buf[0], 0x02);
    EXPECT_EQ(buf[1], 0x01);
}

TEST(ByteBufferTest, PutGetU32) {
    auto buf = ByteBuffer::allocate(10);
    buf.putU32(0x01020304);
    buf.flip();

    EXPECT_EQ(buf.getU32(), 0x01020304);
}

TEST(ByteBufferTest, PutGetU32_LE) {
    auto buf = ByteBuffer::allocate(10);
    buf.order(ByteOrder::LittleEndian);
    buf.putU32(0x01020304);
    buf.flip();

    EXPECT_EQ(buf.getU32(), 0x01020304);
    buf.rewind();
    EXPECT_EQ(buf[0], 0x04);
    EXPECT_EQ(buf[3], 0x01);
}

TEST(ByteBufferTest, PutGetU64) {
    auto buf = ByteBuffer::allocate(16);
    u64 val = 0x0102030405060708ULL;
    buf.putU64(val);
    buf.flip();
    EXPECT_EQ(buf.getU64(), val);
}

// ============================================================================
// 类型化读写 (有符号)
// ============================================================================

TEST(ByteBufferTest, PutGetI16) {
    auto buf = ByteBuffer::allocate(4);
    buf.putI16(-1);
    buf.flip();
    EXPECT_EQ(buf.getI16(), -1);
}

TEST(ByteBufferTest, PutGetI32) {
    auto buf = ByteBuffer::allocate(8);
    buf.putI32(-32768);
    buf.flip();
    EXPECT_EQ(buf.getI32(), -32768);
}

// ============================================================================
// 类型化读写 (浮点)
// ============================================================================

TEST(ByteBufferTest, PutGetF32) {
    auto buf = ByteBuffer::allocate(8);
    buf.putF32(3.14159265f);
    buf.flip();
    EXPECT_FLOAT_EQ(buf.getF32(), 3.14159265f);
}

TEST(ByteBufferTest, PutGetF64) {
    auto buf = ByteBuffer::allocate(16);
    buf.putF64(2.718281828459045);
    buf.flip();
    EXPECT_DOUBLE_EQ(buf.getF64(), 2.718281828459045);
}

// ============================================================================
// 类型化读写 — 绝对
// ============================================================================

TEST(ByteBufferTest, AbsoluteU16) {
    u8 data[4] = {};
    ByteBuffer buf(data, 4);
    buf.putU16(0, 0xAABB);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[1], 0xBB);
    EXPECT_EQ(buf.getU16(0), 0xAABB);
}

TEST(ByteBufferTest, AbsoluteI32) {
    auto buf = ByteBuffer::allocate(8);
    buf.putI32(2, -100);
    EXPECT_EQ(buf.getI32(2), -100);
}

TEST(ByteBufferTest, AbsoluteF64) {
    auto buf = ByteBuffer::allocate(16);
    buf.putF64(4, 1.2345);
    EXPECT_DOUBLE_EQ(buf.getF64(4), 1.2345);
}

// ============================================================================
// 类型化读写 — 边界
// ============================================================================

TEST(ByteBufferTest, U16Underflow) {
    auto buf = ByteBuffer::allocate(1);
    buf.put(0x01);
    buf.flip();
    EXPECT_THROW(buf.getU16(), std::out_of_range);
}

TEST(ByteBufferTest, U16AbsoluteOutOfRange) {
    auto buf = ByteBuffer::allocate(3);
    EXPECT_THROW(buf.getU16(2), std::out_of_range);  // need 2 bytes starting at index 2
}

// ============================================================================
// Slice / Duplicate
// ============================================================================

TEST(ByteBufferTest, Slice) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.put(0x30);
    buf.flip();

    buf.get();  // consume 0x10

    auto sliced = buf.slice();
    EXPECT_EQ(sliced.remaining(), 2);
    EXPECT_EQ(sliced.get(), 0x20);
    EXPECT_EQ(sliced.get(), 0x30);
}

TEST(ByteBufferTest, Duplicate) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0x10);
    buf.put(0x20);
    buf.flip();

    auto dup = buf.duplicate();
    EXPECT_EQ(dup.remaining(), 2);
    EXPECT_EQ(dup.get(), 0x10);
    EXPECT_EQ(buf.get(), 0x10);   // original unaffected
}

// ============================================================================
// 容量管理
// ============================================================================

TEST(ByteBufferTest, Reserve) {
    auto buf = ByteBuffer::allocate(4);
    buf.put(0x41);
    buf.reserve(256);
    EXPECT_GE(buf.capacity(), 256);
    EXPECT_EQ(buf[0], 0x41);
}

TEST(ByteBufferTest, ShrinkToFit) {
    auto buf = ByteBuffer::allocate(100);
    buf.put(0x41);
    buf.put(0x42);
    buf.flip();
    buf.shrinkToFit();
    EXPECT_EQ(buf.capacity(), 2);
    EXPECT_EQ(buf.get(), 0x41);
}

// ============================================================================
// assign / append 兼容
// ============================================================================

TEST(ByteBufferTest, Assign) {
    auto buf = ByteBuffer::allocate(10);
    buf.put(0xFF);

    u8 data[] = {0x10, 0x20};
    buf.assign(data, 2);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[1], 0x20);
}

TEST(ByteBufferTest, Append) {
    auto buf = ByteBuffer::allocate(4);
    buf.put(0x01);

    u8 data[] = {0x02, 0x03};
    buf.append(data, 2);
    buf.flip();

    EXPECT_EQ(buf.get(), 0x01);
    EXPECT_EQ(buf.get(), 0x02);
    EXPECT_EQ(buf.get(), 0x03);
}

TEST(ByteBufferTest, AppendBuffer) {
    auto src = ByteBuffer::allocate(4);
    src.put(0x10);
    src.put(0x20);
    src.flip();

    auto dst = ByteBuffer::allocate(4);
    dst.put(0x01);
    dst.append(src);
    dst.flip();

    EXPECT_EQ(dst.get(), 0x01);
    EXPECT_EQ(dst.get(), 0x10);
    EXPECT_EQ(dst.get(), 0x20);
}

// ============================================================================
// 交换 / 比较
// ============================================================================

TEST(ByteBufferTest, Swap) {
    auto a = ByteBuffer::allocate(4);
    a.put(0x41);

    auto b = ByteBuffer::allocate(4);
    b.put(0x42);

    a.flip();
    b.flip();

    a.swap(b);
    EXPECT_EQ(a.get(), 0x42);
    EXPECT_EQ(b.get(), 0x41);
}

TEST(ByteBufferTest, Equals) {
    u8 a[] = {0x01, 0x02};
    u8 b[] = {0x01, 0x02};
    u8 c[] = {0x01, 0x03};
    EXPECT_TRUE(ByteBuffer(a, 2) == ByteBuffer(b, 2));
    EXPECT_TRUE(ByteBuffer(a, 2) != ByteBuffer(c, 2));
}

TEST(ByteBufferTest, EqualsWithPosition) {
    auto buf  = ByteBuffer::allocate(10);
    buf.put(0xFF);   // filler
    buf.put(0x10);
    buf.put(0x20);
    buf.flip();
    buf.get();       // skip filler

    // remaining: [0x10, 0x20]
    u8 expected[] = {0x10, 0x20};
    EXPECT_TRUE(buf.equals(ByteBuffer(expected, 2)));
}

// ============================================================================
// 大容量写入
// ============================================================================

TEST(ByteBufferTest, LargeWrite) {
    auto buf = ByteBuffer::allocate(32);
    for (int i = 0; i < 100000; ++i) {
        buf.put(static_cast<u8>(i & 0xFF));
    }
    EXPECT_EQ(buf.position(), 100000);
    EXPECT_GE(buf.capacity(), 100000);
}

// ============================================================================
// 混合用例 — 模拟网络协议编解码
// ============================================================================

TEST(ByteBufferTest, ProtocolEncodeDecode) {
    // 模拟一个简单协议头: magic(u16) + length(u32) + type(u8) + payload
    auto buf = ByteBuffer::allocate(32);

    // 写入 (大端网络序)
    buf.putU16(0xCAFE);         // magic
    buf.putU32(4);              // length
    buf.put(0x01);              // type
    buf.putU32(0xDEADBEEF);    // payload

    buf.flip();

    // 读取
    EXPECT_EQ(buf.getU16(), 0xCAFE);
    EXPECT_EQ(buf.getU32(), 4);
    EXPECT_EQ(buf.get(), 0x01);
    EXPECT_EQ(buf.getU32(), 0xDEADBEEF);
}

// ============================================================================
// operator[] 兼容
// ============================================================================

TEST(ByteBufferTest, SubscriptOperator) {
    u8 data[] = {0x10, 0x20, 0x30};
    ByteBuffer buf(data, 3);
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[2], 0x30);
    buf[1] = 0xFF;
    EXPECT_EQ(buf[1], 0xFF);       // 内部缓冲区被修改
    EXPECT_NE(data[1], 0xFF);      // 外部源不受影响 (copyOf 语义)
}

// ============================================================================
// 空缓冲区操作
// ============================================================================

TEST(ByteBufferTest, EmptyOperations) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.remaining(), 0);
    EXPECT_FALSE(buf.hasRemaining());
    EXPECT_THROW(buf.get(), std::out_of_range);
    EXPECT_THROW(buf.position(1), std::out_of_range);
}

// ============================================================================
// ByteOrder 切换
// ============================================================================

TEST(ByteBufferTest, ByteOrderSwitch) {
    auto buf = ByteBuffer::allocate(8);

    // 大端写入，小端读取会出错
    buf.putU16(0x0102);
    buf.putU32(0x03040506);
    buf.flip();

    buf.order(ByteOrder::LittleEndian);
    EXPECT_NE(buf.getU16(), 0x0102);  // 解释为 LE → 0x0201

    buf.rewind();
    buf.order(ByteOrder::BigEndian);
    EXPECT_EQ(buf.getU16(), 0x0102);
}

// ============================================================================
// 有符号整数范围
// ============================================================================

TEST(ByteBufferTest, I32Boundaries) {
    auto buf = ByteBuffer::allocate(16);
    buf.putI32(INT32_MIN);
    buf.putI32(INT32_MAX);
    buf.putI32(0);
    buf.flip();

    EXPECT_EQ(buf.getI32(), INT32_MIN);
    EXPECT_EQ(buf.getI32(), INT32_MAX);
    EXPECT_EQ(buf.getI32(), 0);
}

}  // namespace ca::core
