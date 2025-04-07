#pragma once

#include <string>

#include <vector>
#include <windows.h>
#include <functional>
#include <memory>
#include <map>

namespace ca::ui {

class Control;

class Window
{
public:
    Window(const char* title, int x, int y, int width, int height);
    virtual ~Window() = default;

    void show();
    void hide();

    void setInstance(HINSTANCE hInstance);

    void            registerClass();
    virtual LRESULT handleMessages(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    const char*                           title_;
    int                                   x_;
    int                                   y_;
    int                                   width_;
    int                                   height_;
    HWND                                  hwnd_;
    HINSTANCE                             hInstance_;
    std::vector<std::shared_ptr<Control>> controls_;
};

class WindowManager
{
private:
    std::map<HWND, Window*> windows_;
    WindowManager() = default;

public:

    static WindowManager& getInstance(){
        static WindowManager instance;
        return instance;
	}

    void addWindow(HWND hwnd,Window* window);
    void removeWindow(HWND hwnd);
	Window* getWindow(HWND hwnd);

};

class Control
{
public:
    Control(Window* parent);

    virtual ~Control() = default;

private:
    Window* parent_;
};


class Event
{};

class Button : public Control
{
public:
    Button(Window* parent);
    virtual ~Button() = default;

    Button& setText(const char* text);
    Button& setX(int x);
    Button& setY(int y);
    Button& setWidth(int width);
    Button& setHeight(int height);
    Button& setClickHandler(std::function<void(Event&)> handler);


    static std::shared_ptr<Button> make(Window* parent){
		return std::make_shared<Button>(parent);
	}

private:
    const char* text_;
    int         x_;
    int         y_;
    int         width_;
    int         height_;
    std::function<void(Event&)> clickHandler_;
};

class MessageBox
{
public:
    MessageBox(const char* title, const char* message);
    virtual ~MessageBox() = default;

    static void info(const char* title, const char* message);

    void show();
};


}   // namespace ca::ui
