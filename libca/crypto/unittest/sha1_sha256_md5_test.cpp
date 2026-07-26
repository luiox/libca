#include <gtest/gtest.h>
#include <string>
#include <cstring>

#include "libca/crypto/sha1.hpp"
#include "libca/crypto/sha256.hpp"
#include "libca/crypto/md5.hpp"

using namespace ca::crypto;

// ============================================================
// SHA-1 tests
// ============================================================

TEST(SHA1Test, emptyString) {
    SHA1 sha1;
    EXPECT_EQ(sha1(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(SHA1Test, helloWorld) {
    SHA1 sha1;
    EXPECT_EQ(sha1("Hello World"), "0a4d55a8d778e5022fab701977c5d840bbc486d0");
}

TEST(SHA1Test, abc) {
    SHA1 sha1;
    EXPECT_EQ(sha1("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(SHA1Test, streamingApi) {
    SHA1 sha1;
    sha1.add("Hello ", 6);
    sha1.add("World", 5);
    EXPECT_EQ(sha1.get_hash(), "0a4d55a8d778e5022fab701977c5d840bbc486d0");
}

TEST(SHA1Test, resetReuse) {
    SHA1 sha1;
    EXPECT_EQ(sha1("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
    EXPECT_EQ(sha1(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(SHA1Test, rawBytes) {
    SHA1 sha1;
    unsigned char hash[20];
    sha1.add("abc", 3);
    sha1.get_hash(hash);
    EXPECT_EQ(hash[0], 0xa9);
    EXPECT_EQ(hash[1], 0x99);
    EXPECT_EQ(hash[2], 0x3e);
    EXPECT_EQ(hash[3], 0x36);
}

TEST(SHA1Test, operatorOverload) {
    SHA1 sha1;
    std::string result = sha1("abc");
    EXPECT_EQ(result, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

// ============================================================
// SHA-256 tests
// ============================================================

TEST(SHA256Test, emptyString) {
    SHA256 sha256;
    EXPECT_EQ(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SHA256Test, abc) {
    SHA256 sha256;
    EXPECT_EQ(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SHA256Test, helloWorld) {
    SHA256 sha256;
    EXPECT_EQ(sha256("Hello World"), "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e");
}

TEST(SHA256Test, resetReuse) {
    SHA256 sha256;
    EXPECT_EQ(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// ============================================================
// MD5 tests
// ============================================================

TEST(MD5Test, emptyString) {
    MD5 md5;
    EXPECT_EQ(md5(""), "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(MD5Test, abc) {
    MD5 md5;
    EXPECT_EQ(md5("abc"), "900150983cd24fb0d6963f7d28e17f72");
}

TEST(MD5Test, helloWorld) {
    MD5 md5;
    EXPECT_EQ(md5("Hello World"), "b10a8db164e0754105b7a99be72e3fe5");
}

TEST(MD5Test, resetReuse) {
    MD5 md5;
    EXPECT_EQ(md5("abc"), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(md5(""), "d41d8cd98f00b204e9800998ecf8427e");
}
