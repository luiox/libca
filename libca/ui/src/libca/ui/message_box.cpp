//
// @brief Win32 MessageBox 包装实现
// @author Canrad
// @date 2026/07/20
//

#include "message_box.hpp"

#include "libca/str/charset.hpp"

#include <string>

namespace ca::ui {

core::StatusResult<int> MessageDialog::show() const
{
    auto wide_message = str::CharsetConverter::utf8_to_wide(message_);
    if (wide_message.is_err())
        return core::Err(std::move(wide_message).unwrap_err());
    auto wide_title = str::CharsetConverter::utf8_to_wide(title_);
    if (wide_title.is_err())
        return core::Err(std::move(wide_title).unwrap_err());

    // ::MessageBoxW 在无桌面会话（如 CI 服务账号）下可能返回 0，此时 GetLastError 有值。
    int result = ::MessageBoxW(nullptr,
                               wide_message.unwrap().c_str(),
                               wide_title.unwrap().c_str(),
                               MB_ICONINFORMATION | MB_OK);
    if (result == 0) {
        const DWORD err = GetLastError();
        return core::Err(core::ErrStatus(core::StatusCode::INTERNAL,
                                          "::MessageBoxW failed with Windows error " +
                                              std::to_string(static_cast<unsigned long>(err))));
    }
    return core::Ok(result);
}

core::StatusResult<int> MessageDialog::info(const std::string& title, const std::string& message)
{
    return MessageDialog(title, message).show();
}

}  // namespace ca::ui
