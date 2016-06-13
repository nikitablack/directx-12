#include "..\Headers\Timer.h"
#include "..\Headers\Window.h"
#include "..\Headers\HelloTeapotDemo.h"
#include <wrl/client.h>
#include <memory>
#include <stdexcept>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "D3DCompiler.lib")

using namespace std;
using namespace Microsoft::WRL;
//using namespace DirectX;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const LONG width{ 800 };
	const LONG height{ 600 };
	const UINT bufferCount{ 2 };

	shared_ptr<Window> window;
	shared_ptr<HelloTeapotDemo> teapot;

	try
	{
		window = make_shared<Window>(width, height, "Hello, Teapot!");
		teapot = make_shared<HelloTeapotDemo>(window, bufferCount);
	}
	catch (runtime_error& err)
	{
		MessageBox(nullptr, err.what(), "Error", MB_OK);
		return 0;
	}

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while (msg.message != WM_QUIT)
	{
		BOOL r{ PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) };
		if (r == 0)
		{
			try
			{
				teapot->render();
			}
			catch (runtime_error& err)
			{
				MessageBox(nullptr, err.what(), "Error", MB_OK);
				return 0;
			}
		}
		else
		{
			DispatchMessage(&msg);
		}
	}

	return 0;
}