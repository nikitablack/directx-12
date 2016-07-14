#pragma once

#include <Windows.h>
#include <functional>
#include <memory>

class Window
{
public:
	Window(LONG width, LONG height, LPCSTR title);
	HWND getHandle();
	POINT getMousePosition();
	POINT getSize();
	void addKeyPressCallback(std::shared_ptr<std::function<void(WPARAM)>> onKeyPress);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND hWnd;
	std::shared_ptr<std::function<void(WPARAM)>> onKeyPress;
};