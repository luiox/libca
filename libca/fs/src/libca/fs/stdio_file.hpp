/// @file stdio_file.hpp
/// @brief stdio 文件句柄 —— C stdio 平台打开封装与无预检整读。
///
/// 背景（#249 基准结论）：整读场景 C stdio 大文件比 ifstream 快 3~4x（streambuf
/// 分块拷贝的代价），小/中文件持平；整写场景两者无差，故写路径维持 iostream、
/// 本头只提供读侧设施。平台分支集中于此：Windows 窄 fopen 按 ANSI 代码页解释
/// 路径，UTF-8 中文/emoji 路径必然失败，须走宽字符 _wfopen（经 Path 的编码边界）；
/// POSIX 原生路径即字节序列，直接 fopen。调用方不得散写 _wfopen/fopen。

#pragma once

#include <cstdio>
#include <string>

#include "fs_error.hpp"
#include "path.hpp"
#include "libca/core/result.hpp"

namespace ca { namespace fs {

/// 独占 stdio 文件句柄的 RAII 包装：析构自动 fclose，禁止拷贝、允许移动。
class StdioFile
{
public:
    StdioFile() noexcept = default;
    ~StdioFile() { reset(); }
    StdioFile(const StdioFile&) = delete;
    StdioFile& operator=(const StdioFile&) = delete;

    /// 接管裸句柄的所有权（null 合法，表示空句柄）。
    explicit StdioFile(std::FILE* fp) noexcept : fp_(fp) {}

    StdioFile(StdioFile&& other) noexcept : fp_(other.fp_) { other.fp_ = nullptr; }

    StdioFile& operator=(StdioFile&& other) noexcept
    {
        if (this != &other) {
            reset(other.fp_);
            other.fp_ = nullptr;
        }
        return *this;
    }

    /// 裸句柄（可能为 null）；仅供 stdio API 调用，不得 fclose。
    std::FILE* get() const noexcept { return fp_; }

    bool is_open() const noexcept { return fp_ != nullptr; }

    explicit operator bool() const noexcept { return fp_ != nullptr; }

    /// 释放所有权，返回裸句柄（此后本对象不再负责关闭）。
    std::FILE* release() noexcept
    {
        std::FILE* fp = fp_;
        fp_ = nullptr;
        return fp;
    }

    /// 关闭当前句柄（失败静默——需要检查关闭错误时自行 fclose release() 的句柄），
    /// 并接管新句柄的所有权。
    void reset(std::FILE* fp = nullptr) noexcept
    {
        if (fp_ != nullptr)
            std::fclose(fp_);
        fp_ = fp;
    }

private:
    std::FILE* fp_ = nullptr;
};

/// @brief 平台 stdio 打开辅助：Windows _wfopen / POSIX fopen。
/// @param path 文件路径（编码边界由 Path 承担）。
/// @param mode 打开模式串，仅含 ASCII（"rb"/"wb" 等）。
/// @return 成功返回 RAII 句柄；失败返回 errno 经 classify_error 归类的 FsError
///         （路径不存在为 FileNotFound、无权限为 PermissionDenied 等）。
Result<StdioFile, FsError> open_stdio(const Path& path, const char* mode);

/// @brief 无预检整读：open → seek END 取尺寸 → rewind → 单次 fread。
/// @param path 文件路径。
/// @return 成功返回文件全部字节（空文件为空串）；失败返回 FsError。
/// @note 不做 exists/is_regular_file 预检（每次调用省两次 stat），错误语义由
///       打开/读取的 errno 归类承担：不存在为 FileNotFound；目录在 POSIX 上
///       打开成功、fread 报 EISDIR→IsADirectory，Windows 上打开/读取失败按
///       errno 归类。适合调用方要自行映射错误的整读场景（解析器 read_file 等）；
///       需要 NotARegularFile 明确语义的公开接口请用 FileUtil::read_all_*。
Result<std::string, FsError> read_file_all(const Path& path);

}}  // namespace ca::fs
