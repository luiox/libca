#include <gtest/gtest.h>

#include "libca/crypto/random.hpp"

#include <algorithm>

namespace ca::crypto {

TEST(SecureRandomTest, ZeroLengthSucceeds) {
    auto random = secure_random_bytes(0);
    ASSERT_TRUE(random.is_ok());
    EXPECT_TRUE(random.unwrap().is_empty());
}

TEST(SecureRandomTest, ReturnsTheRequestedLength) {
    for (const ca::usize size : {1, 32, 1025}) {
        SCOPED_TRACE(size);
        auto random = secure_random_bytes(size);
        ASSERT_TRUE(random.is_ok());
        EXPECT_EQ(random.unwrap().len(), size);
    }
}

TEST(SecureRandomTest, FillsTheBufferWithFreshBytes) {
    constexpr ca::usize SIZE = 32;
    auto first_result = secure_random_bytes(SIZE);
    auto second_result = secure_random_bytes(SIZE);
    ASSERT_TRUE(first_result.is_ok());
    ASSERT_TRUE(second_result.is_ok());

    const auto first = first_result.unwrap();
    const auto second = second_result.unwrap();
    ASSERT_EQ(first.len(), SIZE);
    ASSERT_EQ(second.len(), SIZE);

    // 全零或重复的 256 位输出用于捕获未写入和固定输出故障。
    EXPECT_TRUE(std::any_of(first.as_ptr(), first.as_ptr() + SIZE, [](ca::u8 byte) { return byte != 0; }));
    EXPECT_TRUE(std::any_of(second.as_ptr(), second.as_ptr() + SIZE, [](ca::u8 byte) { return byte != 0; }));
    EXPECT_FALSE(std::equal(first.as_ptr(), first.as_ptr() + SIZE, second.as_ptr()));
}

}  // namespace ca::crypto
