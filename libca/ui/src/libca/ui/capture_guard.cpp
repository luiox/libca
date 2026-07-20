//
// @brief 屏幕捕获排除工具实现
// @author Canrad
// @date 2026/07/20
//

#include "capture_guard.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstring>
#include <string>

namespace ca::ui {

namespace {

// SetWindowDisplayAffinity 是 Win7+ API，但 WDA_EXCLUDEFROMCAPTURE 要 Win10 2004+。
// 旧 SDK 没有这个常量，手动定义以兼容老编译器；运行时无效会被 API 拒绝（返回 FALSE）。
#ifndef WDA_EXCLUDEFROMCAPTURE
#    define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

// EnumWindows 的回调签名是 C 风格，需要把目标类名通过 LPARAM 传进来。
// 命中目标类名后调 SetWindowDisplayAffinity，至少成功一次即记为成功。
struct EnumContext
{
    const char* target_class;
    bool        succeeded;
};

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM lparam)
{
    auto* ctx = reinterpret_cast<EnumContext*>(lparam);
    char  window_class[256] = {0};
    if (GetClassNameA(hwnd, window_class, static_cast<int>(sizeof(window_class) - 1)) > 0) {
        if (std::strcmp(window_class, ctx->target_class) == 0) {
            if (SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
                ctx->succeeded = true;
            }
            // 单个窗口失败不中止枚举：可能有多个同类窗口，能成功几个算几个。
        }
    }
    return TRUE;  // 继续枚举
}

}  // namespace

core::Status apply_capture_exclusion(const std::string& class_name)
{
    EnumContext ctx{class_name.c_str(), false};
    EnumWindows(enum_windows_proc, reinterpret_cast<LPARAM>(&ctx));
    if (!ctx.succeeded) {
        return core::ErrStatus(
            core::StatusCode::INTERNAL,
            "apply_capture_exclusion: no window matched or SetWindowDisplayAffinity failed");
    }
    return core::OkStatus();
}

}  // namespace ca::ui
