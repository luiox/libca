#include <gmock/gmock.h>

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <fcntl.h>
#    include <unistd.h>
#endif

#include "libca/io/native_stream.hpp"

namespace ca::io::test {
namespace {

struct RawPipe
{
    RawHandle reader{-1};
    RawHandle writer{-1};
};

RawPipe create_raw_pipe()
{
#if defined(_WIN32)
    HANDLE reader = nullptr;
    HANDLE writer = nullptr;
    if (!CreatePipe(&reader, &writer, nullptr, 0))
        return {};
    return {reinterpret_cast<RawHandle>(reader), reinterpret_cast<RawHandle>(writer)};
#else
    int descriptors[2] = {-1, -1};
    if (::pipe(descriptors) != 0)
        return {};
    return {descriptors[0], descriptors[1]};
#endif
}

bool raw_handle_is_open(RawHandle handle)
{
#if defined(_WIN32)
    DWORD flags = 0;
    return GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) != 0;
#else
    errno = 0;
    return fcntl(static_cast<int>(handle), F_GETFD) != -1 || errno != EBADF;
#endif
}

void close_raw_handle(RawHandle handle)
{
#if defined(_WIN32)
    if (handle != -1)
        CloseHandle(reinterpret_cast<HANDLE>(handle));
#else
    if (handle >= 0)
        ::close(static_cast<int>(handle));
#endif
}

RawHandle create_temporary_file()
{
#if defined(_WIN32)
    wchar_t directory[MAX_PATH]{};
    wchar_t path[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, directory) == 0 || GetTempFileNameW(directory, L"lca", 0, path) == 0)
        return -1;
    HANDLE handle = CreateFileW(path,
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        DeleteFileW(path);
        return -1;
    }
    return reinterpret_cast<RawHandle>(handle);
#else
    char      path[] = "/tmp/libca_io_XXXXXX";
    const int handle = mkstemp(path);
    if (handle >= 0)
        unlink(path);
    return handle;
#endif
}

OwnedHandle adopt_or_fail(RawHandle handle)
{
    auto adopted = OwnedHandle::adopt(handle);
    if (adopted.is_err())
        throw std::runtime_error(adopted.unwrap_err().to_string());
    return std::move(adopted).unwrap();
}

TEST(OwnedHandleTest, RejectsInvalidHandle)
{
    auto invalid = OwnedHandle::adopt(-1);

    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), IoErrorKind::InvalidInput);
}

#if !defined(_WIN32)
TEST(OwnedHandleTest, PosixFileDescriptorZeroIsValid)
{
    auto adopted = OwnedHandle::adopt(0);

    ASSERT_TRUE(adopted.is_ok()) << adopted.unwrap_err().to_string();
    auto handle = std::move(adopted).unwrap();
    EXPECT_EQ(handle.release(), 0);
}
#endif

TEST(OwnedHandleTest, MoveReleaseAndCloseTransferOwnership)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    auto            source = adopt_or_fail(raw.reader);
    const RawHandle saved  = source.get();
    OwnedHandle     target = std::move(source);

    EXPECT_FALSE(source.is_valid());
    EXPECT_TRUE(target.is_valid());
    EXPECT_TRUE(raw_handle_is_open(saved));
    const RawHandle released = target.release();
    EXPECT_FALSE(target.is_valid());
    EXPECT_TRUE(raw_handle_is_open(released));

    auto reacquired = adopt_or_fail(released);
    EXPECT_TRUE(reacquired.close().is_ok());
    EXPECT_FALSE(raw_handle_is_open(released));
    close_raw_handle(raw.writer);
}

TEST(OwnedHandleTest, DuplicateHasIndependentLifetime)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    auto original          = adopt_or_fail(raw.reader);
    auto duplicated_result = original.duplicate();
    ASSERT_TRUE(duplicated_result.is_ok()) << duplicated_result.unwrap_err().to_string();
    auto            duplicated    = std::move(duplicated_result).unwrap();
    const RawHandle duplicate_raw = duplicated.get();

    EXPECT_TRUE(original.close().is_ok());
    EXPECT_TRUE(raw_handle_is_open(duplicate_raw));
    EXPECT_TRUE(duplicated.close().is_ok());
    EXPECT_FALSE(raw_handle_is_open(duplicate_raw));
    close_raw_handle(raw.writer);
}

TEST(OwnedHandleTest, DestructorClosesOwnedResource)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    const RawHandle saved = raw.reader;
    {
        auto handle = adopt_or_fail(raw.reader);
        EXPECT_TRUE(raw_handle_is_open(saved));
    }
    EXPECT_FALSE(raw_handle_is_open(saved));
    close_raw_handle(raw.writer);
}

TEST(NativeStreamTest, AnonymousPipeTransfersDataAndReportsEof)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    NativeStream      reader(adopt_or_fail(raw.reader));
    NativeStream      writer(adopt_or_fail(raw.writer));
    const std::string value = "pipe-data";

    ASSERT_TRUE(writer.write_all(reinterpret_cast<const u8*>(value.data()), value.size()).is_ok());
    ASSERT_TRUE(writer.handle().close().is_ok());
    std::array<u8, 9> buffer{};
    ASSERT_TRUE(reader.read_exact(buffer.data(), buffer.size()).is_ok());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buffer.data()), buffer.size()), value);
    EXPECT_EQ(reader.read(buffer.data(), buffer.size()).unwrap(), 0U);
}

TEST(NativeStreamTest, PipeSeekIsUnsupported)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    NativeStream reader(adopt_or_fail(raw.reader));
    close_raw_handle(raw.writer);

    auto result = reader.seek(SeekFrom::start(0));

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().kind(), IoErrorKind::Unsupported);
}

TEST(NativeStreamTest, TemporaryFileSupportsReadWriteAndSeek)
{
    const RawHandle raw = create_temporary_file();
    ASSERT_NE(raw, -1);
    NativeStream      file(adopt_or_fail(raw));
    const std::string value = "abcdef";
    ASSERT_TRUE(file.write_all(reinterpret_cast<const u8*>(value.data()), value.size()).is_ok());

    EXPECT_EQ(file.seek(SeekFrom::start(2)).unwrap(), 2U);
    u8 middle[2]{};
    ASSERT_TRUE(file.read_exact(middle, sizeof(middle)).is_ok());
    EXPECT_EQ(std::string(reinterpret_cast<char*>(middle), sizeof(middle)), "cd");
    EXPECT_EQ(file.seek(SeekFrom::end(-1)).unwrap(), 5U);
    EXPECT_TRUE(file.rewind().is_ok());
    EXPECT_EQ(file.stream_position().unwrap(), 0U);
}

TEST(NativeStreamTest, IntoHandleEmptiesStream)
{
    auto raw = create_raw_pipe();
    ASSERT_NE(raw.reader, -1);
    NativeStream stream(adopt_or_fail(raw.reader));

    auto handle = stream.into_handle();

    EXPECT_FALSE(stream.is_open());
    EXPECT_TRUE(handle.is_valid());
    EXPECT_TRUE(handle.close().is_ok());
    close_raw_handle(raw.writer);
}

}   // namespace
}   // namespace ca::io::test
