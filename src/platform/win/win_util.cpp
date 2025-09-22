#include "WinPlatform.hpp"
#include <iostream>


// 枚举所有顶级窗口
// usage: 
// EnumWindows(EnumWindowsProc, 0);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    char windowClass[256];
    if (GetClassNameA(hwnd, windowClass, sizeof(windowClass)) > 0) {
        if (strcmp(windowClass, "IntermediateD3DWindowClass") == 0) {
            // 设置窗口显示亲和力，防止被捕获
            if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
                std::cerr << "SetWindowDisplayAffinity failed for window: " << hwnd << std::endl;
            }
            else {
                std::cout << "SetWindowDisplayAffinity succeeded for window: " << hwnd << std::endl;
            }
        }
    }
    return TRUE; // 继续枚举窗口
}