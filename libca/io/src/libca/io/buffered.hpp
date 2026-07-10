#pragma once

#include <memory>
#include <vector>

#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"

namespace ca::io {

/// @brief 拥有底层 Reader 的固定容量缓冲读取器。
///
/// BufReader 优先返回已经预读的数据。缓冲为空且请求不小于缓冲容量时直接读取到底层，
/// 避免一次额外复制。fill_buf() 返回的视图在下一次可变操作前有效。
class BufReader final : public Reader
{
public:
    BufReader(const BufReader&)            = delete;
    BufReader& operator=(const BufReader&) = delete;
    BufReader(BufReader&& other) noexcept;
    BufReader& operator=(BufReader&& other) noexcept;
    ~BufReader() override = default;

    /// @brief 创建拥有 inner 的缓冲读取器；inner 为空或 capacity 为 0 时返回 InvalidInput。
    static IoResult<BufReader> create(std::unique_ptr<Reader> inner, usize capacity = 8192);

    IoResult<usize> read(u8* buffer, usize capacity) override;

    /// @brief 返回当前可用缓冲视图；缓冲为空时从底层读取一次。
    IoResult<ca::core::ByteSlice> fill_buf();

    /// @brief 消费 fill_buf() 返回视图的前 amount 个字节。
    IoResult<void> consume(usize amount);

    /// @brief 追加读取到 delimiter（包含分隔符）或 EOF，返回本次追加长度。
    IoResult<usize> read_until(u8 delimiter, ca::core::BytesMut& output);

    /// @brief 返回缓冲容量。
    usize capacity() const noexcept;

    /// @brief 返回当前已预读但尚未消费的字节数。
    usize buffered_len() const noexcept;

    /// @brief 返回底层 Reader；移动后的对象返回 nullptr。
    Reader*       inner() noexcept;
    const Reader* inner() const noexcept;

    /// @brief 取回底层 Reader 并使当前对象变为空对象。
    /// @warning 尚未消费的预读字节会被丢弃。
    std::unique_ptr<Reader> into_inner() noexcept;

private:
    BufReader(std::unique_ptr<Reader> inner, std::vector<u8> buffer) noexcept;

    std::unique_ptr<Reader> inner_;
    std::vector<u8>         buffer_;
    usize                   position_{0};
    usize                   filled_{0};
};

/// @brief 拥有底层 Writer 的固定容量缓冲写入器。
///
/// flush() 是可观察错误的正常关闭路径。析构函数仅 best-effort 写出本地缓冲，不调用底层
/// flush()，业务代码不能依赖析构确认写入成功。
class BufWriter final : public Writer
{
public:
    BufWriter(const BufWriter&)            = delete;
    BufWriter& operator=(const BufWriter&) = delete;
    BufWriter(BufWriter&& other) noexcept;
    BufWriter& operator=(BufWriter&& other) noexcept;
    ~BufWriter() override;

    /// @brief 创建拥有 inner 的缓冲写入器；inner 为空或 capacity 为 0 时返回 InvalidInput。
    static IoResult<BufWriter> create(std::unique_ptr<Writer> inner, usize capacity = 8192);

    IoResult<usize> write(const u8* data, usize length) override;
    IoResult<void>  flush() override;

    /// @brief 显式 flush 后取回底层 Writer；失败时仍保留底层对象和未写缓冲。
    IoResult<std::unique_ptr<Writer>> finish();

    /// @brief 返回缓冲容量。
    usize capacity() const noexcept;

    /// @brief 返回尚未写到底层的字节数。
    usize buffered_len() const noexcept;

    /// @brief 返回底层 Writer；移动后或 finish 成功后返回 nullptr。
    Writer*       inner() noexcept;
    const Writer* inner() const noexcept;

private:
    BufWriter(std::unique_ptr<Writer> inner, std::vector<u8> buffer) noexcept;

    IoResult<void> flush_buffer();
    void           discard_written_prefix(usize count) noexcept;

    std::unique_ptr<Writer> inner_;
    std::vector<u8>         buffer_;
    usize                   used_{0};
};

}   // namespace ca::io
