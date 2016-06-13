#include "..\Headers/Window.h"
#include <stdexcept>

using namespace std;

Window::Window(LONG width, LONG height, LPCSTR title)
{
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = &WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = (HMODULE)GetModuleHandle(0);
	wcex.hIcon = LoadIcon(NULL, IDI_SHIELD);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = "WindowClass";
	wcex.hIconSm = LoadIcon(NULL, IDI_WARNING);

	if (RegisterClassEx(&wcex) == 0)
	{
		throw(runtime_error{ "Error registering window." });
	}

	RECT rect{ 0, 0, width, height };
	AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

	hWnd = CreateWindowEx(0, "WindowClass", title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, nullptr, this);
	if (!hWnd)
	{
		throw(runtime_error{ "Error creating window." });
	}

	ShowWindow(hWnd, SW_SHOW);

	/*MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}*/
}

HWND Window::getHandle()
{
	return hWnd;
}

POINT Window::getMousePosition()
{
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hWnd, &p);
	return p;
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Window* window{ reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)) };

	switch (message)
	{
	case WM_CREATE:
		{
			LPCREATESTRUCT pCreateStruct{ reinterpret_cast<LPCREATESTRUCT>(lParam) };
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		}
		break;
	/*case WM_PAINT:
		if (window)
		{
			PAINTSTRUCT ps;
			HDC dc{ BeginPaint(hWnd, &ps) };

			// do drawing to 'dc' here -- or don't

			EndPaint(hWnd, &ps);
		}
		break;*/
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}