#include <windows.h>

//6. 窗口的消息处理函数
//自定义的消息处理函数
LRESULT CALLBACK myWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


//int APIENTRY WinMain(
//	HINSTANCE hInst,			//当前应用程序实例句柄
//	HINSTANCE HPrevInstance,		//前一个应用程序实例句柄
//	LPCTSTR	  lpCmdLine,			//命令行  空格隔开多个字符串 gets
//	int		  nCmdShow)//显示方式



int WINAPI wWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine, 
	int nCmdShow) {			
	//1. 注册窗口类	注册公司
	WNDCLASS wc = { 0 };
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hbrBackground = NULL;
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = hInstance;//WinMain第一个参数
	wc.lpfnWndProc = myWndProc;//自定义的消息处理函数
	wc.lpszClassName = L"MyWindowClass";//类名
	wc.lpszMenuName = NULL;//菜单名
	wc.style = CS_HREDRAW | CS_VREDRAW;//水平 垂直 重绘 Re Draw 

	RegisterClass(&wc);
	//2. 创建窗口
	HWND hWnd = CreateWindowEx(wc.style,
		wc.lpszClassName,
		L"第1个窗口",
		WS_OVERLAPPEDWINDOW,
		100, 100,
		600, 600,
		NULL, NULL, wc.hInstance,
		NULL);
	if (!hWnd) return 0;
	//3. 显示窗口
	ShowWindow(hWnd, SW_SHOW);
	//4. 刷新窗口
	UpdateWindow(hWnd);
	//5. 消息循环
	MSG msg;
	while (1) {
		//5.1 获取消息
		GetMessage(&msg, NULL, NULL, NULL);
		//5.2 翻译消息
		TranslateMessage(&msg);
		//5.3 派发消息
		DispatchMessage(&msg);
	}

	return 0;
}

//6. 窗口的消息处理函数
//自定义的消息处理函数
LRESULT CALLBACK myWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
