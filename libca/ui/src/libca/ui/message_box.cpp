//
// @brief Win32 MessageBox 包装实现
// @author Canrad
// @date 2026/07/20
//

#include "message_box.hpp"

#include <string>

namespace ca::ui {

namespace {

// UTF-8 → UTF-16，返回 std::wstring。失败时返回空串——::MessageBoxW 仍能弹出空标题。
std::wstring to_wide(const std::string& narrow)
{
    if (narrow.empty())
        return std::wstring{};
    int wide_len = MultiByteToWideChar(
        CP_UTF8, 0, narrow.data(), static_cast<int>(narrow.size()), nullptr, 0);
    std::wstring wide(static_cast<usize>(wide_len > 0 ? wide_len : 0), L'\0');
    if (wide_len > 0)
        MultiByteToWideChar(CP_UTF8,
                            0,
                            narrow.data(),
                            static_cast<int>(narrow.size()),
                            &wide[0],
                            wide_len);
    return wide;
}

}  // namespace

core::StatusResult<int> MessageBox::show() const
{
    // ::MessageBoxW 在无桌面会话（如 CI 服务账号）下可能返回 0，此时 GetLastError 有值。
    int result = ::MessageBoxW(nullptr,
                               to_wide(message_).c_str(),
                               to_wide(title_).c_str(),
                               MB_ICONINFORMATION | MB_OK);
    if (result == 0) {
        const DWORD err = GetLastError();
        return core::Err(core::ErrStatus(core::StatusCode::INTERNAL,
                                          "::MessageBoxW failed with Windows error " +
                                              std::to_string(static_cast<unsigned long>(err))));
    }
    return core::Ok(result);
}

core::StatusResult<int> MessageBox::info(const std::string& title, const std::string& message)
{
    return MessageBox(title, message).show();
}

}  // namespace ca::ui
