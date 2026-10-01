#pragma once

/// @file zip_error.hpp
/// @brief zip 模块错误码与带详情的错误值。

#include <string>

namespace ca::zip {

/// @brief zip 模块通用错误码。
enum class ZipError {
    NOT_FOUND,          ///< 条目或归档文件不存在
    INVALID_FORMAT,     ///< 结构损坏：签名不符、字段越界、流截断、ZIP64 extra 非法
    CRC_MISMATCH,       ///< CRC32 / ISIZE / 头 CRC16 校验失败
    UNSUPPORTED,        ///< 分卷/多磁盘 ZIP、不支持的压缩方法、保留 flag、超 32 位表示上限
    IO_FAILED,          ///< fopen/fread/fwrite 等文件读写失败
    ZLIB_ERROR,         ///< zlib 初始化或流处理返回异常码（非数据问题）
    INVALID_ARGUMENT,   ///< 参数非法（如压缩级别越界）
    INVALID_STATE,      ///< 状态机误用：未打开即写、无当前条目、已 finish 再写、重复 open
};

/// @brief 将 ZipError 转为稳定的调试字符串。
/// @param error zip 错误码。
/// @return 错误描述字符串。
inline const char* to_string(ZipError error) noexcept {
    switch (error) {
    case ZipError::NOT_FOUND: return "entry or archive not found";
    case ZipError::INVALID_FORMAT: return "invalid archive format";
    case ZipError::CRC_MISMATCH: return "checksum mismatch";
    case ZipError::UNSUPPORTED: return "unsupported archive feature";
    case ZipError::IO_FAILED: return "file io failed";
    case ZipError::ZLIB_ERROR: return "zlib internal error";
    case ZipError::INVALID_ARGUMENT: return "invalid argument";
    case ZipError::INVALID_STATE: return "invalid state";
    }
    return "unknown zip error";
}

/// @brief 带详情的错误值：code 供程序分支，message 供诊断与测试断言。
/// @note 作为 zip 模块公开接口的 Result 错误类型；常规失败一律经 Result 返回，
///       异常仅保留在无法返回值的构造重载与 get_entry_at 等编程错误路径。
struct ZipErrorInfo {
    /// 错误码。
    ZipError code = ZipError::INVALID_FORMAT;
    /// 人读详情（UTF-8，含动态名如条目名/路径/zlib 状态码）。
    std::string message;
};

}   // namespace ca::zip
