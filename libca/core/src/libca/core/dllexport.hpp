#pragma once

/// @file dllexport.hpp
/// @brief 动态库导出/导入宏 LIBCA_API。
/// @note 当前只覆盖 MSVC/Windows 的 __declspec 语义，跨平台 visibility 后续可扩展。

// ============================================================================
// DLL 导出/导入宏
// ============================================================================
// 当 LIBCA_DLL_MODE 定义时，使用动态链接库模式：
//   - LIBCA_DLL_EXPORT 定义时：导出符号
//   - LIBCA_DLL_EXPORT 未定义时：导入符号
// 未定义 LIBCA_DLL_MODE 时，所有符号均为普通可见性（静态库模式）。
// ============================================================================

#ifdef LIBCA_DLL_MODE
#    ifdef LIBCA_DLL_EXPORT
#        define LIBCA_API __declspec(dllexport)
#    else
#        define LIBCA_API __declspec(dllimport)
#    endif
#else
#    define LIBCA_API
#endif
