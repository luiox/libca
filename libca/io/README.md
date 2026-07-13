# libca_io

统一 IO 抽象：Reader / Writer / Seek 接口、缓冲读写、原生 handle/fd RAII。命名空间 `ca::io`。

> 设计与选型见 `doc/design.md`；以下为快速示例。接口签名见头文件 Doxygen 注释。

## 读取字节流

实现 `Reader` 只需提供一次读取。EOF 返回 `Ok(0)`，短读是正常结果：

```cpp
class PipeReader final : public ca::io::Reader
{
public:
    ca::io::IoResult<usize> read(u8* buffer, usize capacity) override;
};
```

需要固定长度时用 `read_exact()`，不要手写一次 read 再检查长度：

```cpp
u8 header[16]{};
auto result = reader.read_exact(header, sizeof(header));
```

## 写入完整数据

`Writer::write()` 允许短写。发送整个消息用 `write_all()`：

```cpp
const std::string message = "request\n";
writer.write_all(reinterpret_cast<const u8*>(message.data()), message.size());
```

## 缓冲读写

```cpp
auto source = std::make_unique<PipeReader>(std::move(pipe));
auto reader = std::move(ca::io::BufReader::create(std::move(source), 8192)).unwrap();

ca::core::BytesMut line = ca::core::BytesMut::with_capacity(128);
reader.read_until(static_cast<u8>('\n'), line);
```

`fill_buf()` 返回非拥有视图；调用 `consume()`、再次读取、移动或销毁 BufReader 后，旧视图失效。`into_inner()` 会丢弃尚未消费的预读字节。

```cpp
auto sink = std::make_unique<PipeWriter>(std::move(pipe));
auto writer = std::move(ca::io::BufWriter::create(std::move(sink), 8192)).unwrap();
writer.write_all(payload.data(), payload.size());
writer.flush();   // 必须检查结果；析构只能 best-effort 写出
```

## 接管原生 handle/fd

```cpp
auto adopted = ca::io::OwnedHandle::adopt(raw_handle);
ca::io::NativeStream stream(std::move(adopted).unwrap());
```

`adopt()` 后不得再由原代码关闭该资源。需要交回所有权用 `release()`；需要另一个独立所有者用 `duplicate()`。Windows `SOCKET` 不能交给 `OwnedHandle`（须由 `closesocket()` 关闭）。

## 定位

```cpp
stream.seek(ca::io::SeekFrom::start(0));
stream.seek(ca::io::SeekFrom::end(-16));
stream.rewind();
```

并非所有流都支持 Seek；管道和 socket 返回 `Unsupported`。
