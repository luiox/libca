#pragma once

#include <string>

#include "libca/core/status.hpp"

/// @file minidump.hpp
/// @brief Windows 崩溃转储（minidump）：安装未处理异常过滤器，崩溃时经 dbghelp
///        MiniDumpWriteDump 写出 "<prefix>-<pid>-<时间戳>.dmp"。
/// @note 仅 Windows 生效；其他平台头文件可正常包含，运行时接口返回"不支持"错误。

namespace ca::core {

/// @brief 安装崩溃转储过滤器：SetUnhandledExceptionFilter + dbghelp MiniDumpWriteDump。
/// @param path_prefix 转储文件路径前缀（UTF-8）。崩溃时写出
///                    "<path_prefix>-<pid>-<epoch 毫秒>.dmp"，目录需已存在。
/// @return 成功返回 OK；path_prefix 为空或过长返回 INVALID_ARGUMENT。
/// @note 幂等：已安装状态下重复调用只更新 path_prefix，不会重复覆盖过滤器链；
///       uninstall_crash_dump() 会恢复 install 之前的过滤器。
/// @note dump 级别为 MiniDumpNormal：体积小、崩溃路径上落盘最快，含线程栈与
///       模块信息；不含堆内存，如需诊断堆内容可改用 MiniDumpWithFullMemory
///       （代价是转储耗时与文件体积大幅增加，崩溃现场可能"死得更久"）。
/// @note dbghelp 线程约束：dbghelp 不是线程安全的，MiniDumpWriteDump 与符号
///       诊断（SymInitialize/SymFromAddr 等，见 stacktrace.hpp）并发调用会产生
///       未定义行为，须在外部保证互斥；崩溃过滤器本身运行在崩溃线程的异常
///       上下文中，与业务线程是天然串行的。
/// @note 过滤器内部只用栈上缓冲与 Win32 调用，不做可能失败的堆分配；写完 dump
///       后返回 EXCEPTION_EXECUTE_HANDLER 终止进程，不链式调用先前过滤器。
Status install_crash_dump(const std::string& path_prefix);

/// @brief 卸载崩溃转储过滤器：恢复 install 之前的异常过滤器（未安装时为系统默认）。
/// @return 幂等：未安装时同样返回 OK。
Status uninstall_crash_dump();

/// @brief 立即把当前进程的 minidump 写到指定路径（UTF-8，建议 .dmp 后缀）。
/// @return 成功返回 OK；路径为空/非法 UTF-8 返回 INVALID_ARGUMENT，
///         建文件或写 dump 失败返回 INTERNAL。
/// @note 与崩溃路径同为 MiniDumpNormal，但不含异常记录；供测试与主动取证使用。
///       调用方须保证不与崩溃过滤器及符号化并发调用（见 dbghelp 线程约束）。
Status write_minidump(const std::string& path);

/// @brief 当前是否已安装崩溃转储过滤器。
bool is_crash_dump_installed();

} // namespace ca::core
