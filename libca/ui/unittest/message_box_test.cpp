#include <gmock/gmock.h>

#include <libca/core/status.hpp>
#include <libca/ui/message_box.hpp>

#include <cstdlib>
#include <string>

namespace ca::ui {

namespace {

// 默认跳过实际弹窗：::MessageBoxW 在交互桌面下会阻塞等待用户点击，
// CI 与本地自动化跑都会卡死。只有显式设置 LIBCA_UI_INTERACTIVE=1 时才弹窗。
// 也可以传 --gmock_filter=-* 让所有测试空跑。
bool interactive_enabled()
{
    const char* v = std::getenv("LIBCA_UI_INTERACTIVE");
    return v != nullptr && std::string(v) == "1";
}

}  // namespace

// 默认跳过，避免 ::MessageBoxW 在交互桌面阻塞 CI。
// 设置 LIBCA_UI_INTERACTIVE=1 时才真正弹窗，验证 StatusResult 形态。
TEST(MessageDialogTest, InfoReturnsStatusResult) {
    if (!interactive_enabled()) {
        GTEST_SKIP() << "skip interactive MessageDialog; set LIBCA_UI_INTERACTIVE=1 to enable";
    }
    auto result = MessageDialog::info("libca_ui_test", "test message");
    EXPECT_TRUE(result.is_ok());
}

TEST(MessageDialogTest, RejectsInvalidUtf8WithoutOpeningDialog) {
    auto result = MessageDialog::info("\xFF", "test message");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code(), core::StatusCode::INVALID_ARGUMENT);
}

}  // namespace ca::ui
