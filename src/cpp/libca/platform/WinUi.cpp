#include "winui.hpp"

namespace ca::ui {

Window::Window(const char* title, int x, int y, int width, int height)
    : title_(title)
    , x_(x)
    , y_(y)
    , width_(width)
    , height_(height)
    , hInstance_(nullptr)
{
    registerClass();
    hwnd_ = CreateWindow(TEXT("libca::ui::WindowClass"),
                         TEXT("title"),
                         WS_OVERLAPPEDWINDOW,
                         x,
                         y,
                         width,
                         height,
                         nullptr,
                         nullptr,
                         hInstance_,
                         nullptr);
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

LRESULT CALLBACK GlobalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto obj = WindowManager::getInstance().getWindow(hwnd);
    if (obj != nullptr) {
        return obj->handleMessages(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::handleMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefDlgProc(hwnd, uMsg, wParam, lParam);
}

void Window::registerClass()
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = GlobalWindowProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance_;
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = TEXT("libca::ui::WindowClass");
    wcex.hIconSm       = 0;

    RegisterClassExW(&wcex);
}

void Window::setInstance(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
}

void Window::show()
{
    ShowWindow(hwnd_, SW_SHOW);
    MSG msg;
    // 主消息循环:
    do {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } while (true);
}

void WindowManager::addWindow(HWND hwnd, Window* window)
{
    windows_[hwnd] = window;
}

void WindowManager::removeWindow(HWND hwnd)
{
    windows_.erase(hwnd);
}

Window* WindowManager::getWindow(HWND hwnd)
{
    return windows_[hwnd];
}

Control::Control(Window* parent)
    : parent_(parent)
{}

Button::Button(Window* parent)
    : Control(parent)
{}

Button& Button::setText(const char* text)
{
    text_ = text;
    return *this;
}
Button& Button::setX(int x)
{
    x_ = x;
    return *this;
}
Button& Button::setY(int y)
{
    y_ = y;
    return *this;
}
Button& Button::setWidth(int width)
{
    width_ = width;
    return *this;
}
Button& Button::setHeight(int height)
{
    height_ = height;
    return *this;
}
Button& Button::setClickHandler(std::function<void(Event&)> handler)
{
    clickHandler_ = handler;
    return *this;
}

MessageBox::MessageBox(const char* title, const char* message) {}

void MessageBox::info(const char* title, const char* message) {}

void show() {}

}   // namespace ca::ui