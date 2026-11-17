#pragma once

// libca/io：统一同步字节流协议与缓冲层。
// 模块设计与取舍见 doc:io-design。

#include "libca/io/buffered.hpp"
#include "libca/io/error.hpp"
#include "libca/io/native_handle.hpp"
#include "libca/io/native_stream.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/seek.hpp"
#include "libca/io/writer.hpp"
