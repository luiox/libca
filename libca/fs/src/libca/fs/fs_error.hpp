#pragma once

#include <string>

namespace ca {
namespace fs {

/// fs 模块文件操作错误码（中等粒度）。
///
/// 作为 Result<T, FsError> 的错误类型，调用方可用 to_string() 转可读字符串打印。
enum class FsError
{
    Ok = 0,              ///< 无错误
    FileNotFound,        ///< 路径不存在
    NotARegularFile,     ///< 读文件时目标非普通文件
    NotADirectory,       ///< 遍历/目录操作时目标非目录
    OpenFailed,          ///< 文件打开失败
    ReadFailed,          ///< 读取失败
    WriteFailed,         ///< 写入失败
    PermissionDenied,    ///< 权限不足
    AlreadyExists,       ///< CREATE_NEW 模式下文件已存在
    DiskFull,            ///< 磁盘空间不足（写满）
    IsADirectory,        ///< 期望文件却是目录
    DirectoryNotEmpty,   ///< 删除/覆盖非空目录
    NameTooLong,         ///< 路径或文件名过长
    TooManyOpenFiles,    ///< 进程/系统打开文件数超限
    Unknown,             ///< 未分类异常
};

/// FsError 转人类可读字符串，便于打印日志。
std::string to_string(FsError e);

}   // namespace fs
}   // namespace ca
