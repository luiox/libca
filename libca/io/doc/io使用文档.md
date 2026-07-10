---
version: 1.0
update:
2026-07-11 - 完成 libca_io 第一版使用文档
---

# libca_io 使用文档

## 读取字节流

实现 `Reader` 时只需要提供一次读取操作。EOF 返回 `Ok(0)`，短读是正常结果：

```cpp
class PipeReader final : public ca::io::Reader
{
public:
    ca::io::IoResult<usize> read(u8* buffer, usize capacity) override;
};
```

调用方需要固定长度时使用 `read_exact()`，不要手写一次 read 后检查长度：

```cpp
u8 header[16]{};
auto result = reader.read_exact(header, sizeof(header));
if (result.is_err()) {
    handle_io_error(result.unwrap_err());
}
```

## 写入完整数据

`Writer::write()` 允许短写。需要发送整个消息时使用 `write_all()`：

```cpp
const std::string message = "request\n";
auto result = writer.write_all(
    reinterpret_cast<const u8*>(message.data()), message.size());
```

`write_all()` 会重试 `Interrupted` 和短写，但会保留 `WouldBlock` 等需要调用方处理的错误。

## 使用缓冲读取

缓冲器拥有底层 Reader：

```cpp
auto source = std::make_unique<PipeReader>(std::move(pipe));
auto created = ca::io::BufReader::create(std::move(source), 8192);
auto reader = std::move(created).unwrap();

ca::core::BytesMut line = ca::core::BytesMut::with_capacity(128);
auto count = reader.read_until(static_cast<u8>('\n'), line);
```

`fill_buf()` 返回非拥有视图。调用 `consume()`、再次读取、移动或销毁 BufReader 后，不得再
访问旧视图。

`into_inner()` 会返回底层 Reader，但会丢弃尚未消费的预读字节。只有确认缓冲为空，或
调用方明确不再需要这些字节时才能使用。

## 使用缓冲写入

```cpp
auto sink = std::make_unique<PipeWriter>(std::move(pipe));
auto created = ca::io::BufWriter::create(std::move(sink), 8192);
auto writer = std::move(created).unwrap();

writer.write_all(payload.data(), payload.size());
auto flushed = writer.flush();
```

必须检查显式 `flush()` 的结果。析构时只能 best-effort 写出缓冲，无法向调用方报告错误。

## 接管原生 handle 或 fd

```cpp
auto adopted = ca::io::OwnedHandle::adopt(raw_handle);
if (adopted.is_err()) {
    handle_io_error(adopted.unwrap_err());
    return;
}

ca::io::NativeStream stream(std::move(adopted).unwrap());
```

`adopt()` 后不得再由原代码关闭该资源。需要交回所有权时调用 `release()`；需要另一个独立
所有者时调用 `duplicate()`。

Windows `SOCKET` 不能交给 `OwnedHandle`，因为它必须由 `closesocket()` 关闭。

## 定位文件流

```cpp
stream.seek(ca::io::SeekFrom::start(0));
stream.seek(ca::io::SeekFrom::current(32));
stream.seek(ca::io::SeekFrom::end(-16));
stream.rewind();
```

并非所有字节流都支持 Seek。管道和 socket 返回 `IoErrorKind::Unsupported`。

## 处理错误

```cpp
auto result = reader.read(buffer, capacity);
if (result.is_err()) {
    const ca::io::IoError error = result.unwrap_err();
    if (error.kind() == ca::io::IoErrorKind::WouldBlock) {
        wait_until_readable();
    }
}
```

诊断日志应记录 `error.to_string()`，其中包含稳定类别、操作上下文和可用的原生错误码。
