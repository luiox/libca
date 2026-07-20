#include <gmock/gmock.h>

#include <libca/core/status.hpp>
#include <libca/ui/button.hpp>
#include <libca/ui/window.hpp>

namespace ca::ui {
namespace {

void remove_quit_messages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
}

class TrackingWindow final : public Window
{
public:
    TrackingWindow()
        : Window("", CW_USEDEFAULT, CW_USEDEFAULT, 320, 200)
    {}

    LRESULT handle_messages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
    {
        if (msg == WM_CREATE)
            saw_create = true;
        return Window::handle_messages(hwnd, msg, wparam, lparam);
    }

    bool saw_create{false};
};

TEST(WindowTest, RoutesCreationAndButtonMessagesAndReleasesHandles)
{
    HWND window_handle = nullptr;
    HWND button_handle = nullptr;
    {
        TrackingWindow window;
        auto           created = window.create(GetModuleHandleW(nullptr));
        ASSERT_TRUE(created.is_ok()) << created.to_string();
        ASSERT_TRUE(window.saw_create);
        window_handle = window.native_handle();
        ASSERT_NE(window_handle, nullptr);
        EXPECT_EQ(WindowManager::get_instance().get_window(window_handle), &window);

        bool clicked = false;
        auto button  = Button::make(&window);
        button->set_text("").set_click_handler([&](ClickEvent&) { clicked = true; });
        auto button_created = button->create();
        ASSERT_TRUE(button_created.is_ok()) << button_created.to_string();
        button_handle = button->native_handle();
        ASSERT_NE(button_handle, nullptr);
        window.add_control(button);

        SendMessageW(window_handle,
                     WM_COMMAND,
                     MAKEWPARAM(0, BN_CLICKED),
                     reinterpret_cast<LPARAM>(button_handle));
        EXPECT_TRUE(clicked);

        auto duplicate = button->create();
        ASSERT_TRUE(duplicate.is_err());
        EXPECT_EQ(duplicate.code(), core::StatusCode::ALREADY_EXISTS);
    }

    EXPECT_FALSE(IsWindow(button_handle));
    EXPECT_FALSE(IsWindow(window_handle));
    EXPECT_EQ(WindowManager::get_instance().get_window(window_handle), nullptr);
    remove_quit_messages();
}

TEST(WindowTest, RejectsButtonBeforeParentCreation)
{
    Window window("parent", CW_USEDEFAULT, CW_USEDEFAULT, 320, 200);
    auto   button  = Button::make(&window);
    auto   created = button->create();
    ASSERT_TRUE(created.is_err());
    EXPECT_EQ(created.code(), core::StatusCode::FAILED_PRECONDITION);
}

}   // namespace
}   // namespace ca::ui
