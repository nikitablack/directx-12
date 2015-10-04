#include <Windows.h>
#include <wrl/client.h>
#include <stdexcept>
#include <dxgi1_4.h>
#include <d3d12.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

using namespace std;
using Microsoft::WRL::ComPtr;

const LONG WIDTH{ 800 };
const LONG HEIGHT{ 600 };
const UINT BUFFER_COUNT{ 2 };

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			PostMessage(hWnd, WM_DESTROY, 0, 0);
			return 0;
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = (HMODULE)GetModuleHandle(0);
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = "WindowClass";
	wcex.hIconSm = nullptr;

	if(!RegisterClassEx(&wcex))
	{
		MessageBox(nullptr, "Error registering window.", "Error", MB_OK);
		return 0;
	}

	RECT rect{ 0, 0, WIDTH, HEIGHT };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	const int windowWidth{ (rect.right - rect.left) };
	const int windowHeight{ (rect.bottom - rect.top) };

	HWND hWnd{ CreateWindow("WindowClass", "DirectX 12 Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, windowWidth, windowHeight, nullptr, nullptr, nullptr, nullptr) };
	if(!hWnd)
	{
		MessageBox(nullptr, "Error creating window.", "Error", MB_OK);
		return 0;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	ComPtr<IDXGIFactory2> dxgiFactory2;
	UINT factoryFlags{ 0 };
#if _DEBUG
	factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	HRESULT hr{ CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory2)) };
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating IDXGIFactory2.", "Error", MB_OK);
		return 0;
	}

	ComPtr<IDXGIFactory4> dxgiFactory;
	hr = dxgiFactory2.As(&dxgiFactory);
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating IDXGIFactory4.", "Error", MB_OK);
		return 0;
	}

#ifdef _DEBUG
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	ComPtr<IDXGIAdapter1> adapterTemp;
	ComPtr<IDXGIAdapter3> adapter;

	for (UINT adapterIndex{ 0 }; dxgiFactory->EnumAdapters1(adapterIndex, adapterTemp.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		ZeroMemory(&desc, sizeof(desc));

		adapterTemp->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		hr = adapterTemp.As(&adapter);
		if (SUCCEEDED(hr))
		{
			break;
		}
		else
		{
			adapter = nullptr;
		}
	}

	if (adapter == nullptr)
	{
		MessageBox(nullptr, "Error grtting hardware adapter.", "Error", MB_OK);
		return 0;
	}

	ComPtr<ID3D12Device> device;
	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating device.", "Error", MB_OK);
		return 0;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(queueDesc));
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ComPtr<ID3D12CommandQueue> commandQueue;
	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating command queue.", "Error", MB_OK);
		return 0;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = windowWidth;
	swapChainDesc.Height = windowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	ComPtr<IDXGISwapChain1> swapChain1;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating swap chain.", "Error", MB_OK);
		return 0;
	}

	ComPtr<IDXGISwapChain3> swapChain;
	hr = swapChain1.As(&swapChain);
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating IDXGISwapChain3.", "Error", MB_OK);
		return 0;
	}

	ComPtr<ID3D12CommandAllocator> commandAlloc;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAlloc.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating command allocator.", "Error", MB_OK);
		return 0;
	}

	ComPtr<ID3D12GraphicsCommandList> commandList;
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAlloc.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating command list.", "Error", MB_OK);
		return 0;
	}

	ComPtr<ID3D12Resource> buffers[BUFFER_COUNT];
	for (UINT i{ 0 }; i < BUFFER_COUNT; i++)
	{
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(buffers[i].ReleaseAndGetAddressOf()));
		if (FAILED(hr))
		{
			MessageBox(nullptr, "Error getting buffer.", "Error", MB_OK);
			return 0;
		}

		buffers[i]->SetName(L"SwapChain_Buffer");
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = 10;
	heapDesc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> descHeapRtv;
	hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descHeapRtv.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating descriptor heap.", "Error", MB_OK);
		return 0;
	}

	UINT rtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	for (UINT i{ 0 }; i < BUFFER_COUNT; i++)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE d{ descHeapRtv->GetCPUDescriptorHandleForHeapStart() };
		d.ptr += i * rtvStep;
		device->CreateRenderTargetView(buffers[i].Get(), nullptr, d);
	}

	ComPtr<ID3D12Fence> fence;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()));
	if (FAILED(hr))
	{
		MessageBox(nullptr, "Error creating fence.", "Error", MB_OK);
		return 0;
	}

	HANDLE fenceEveneHandle{ CreateEvent(nullptr, FALSE, FALSE, nullptr) };

















	uint64_t frameCount{ 0 };

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	while(msg.message != WM_QUIT)
	{
		BOOL r{ PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) };
		if(r == 0)
		{
			++frameCount;

			// Get current RTV descriptor
			UINT descHandleRtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
			D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv{ descHeapRtv->GetCPUDescriptorHandleForHeapStart() };
			descHandleRtv.ptr += ((frameCount - 1) % BUFFER_COUNT) * descHandleRtvStep;

			// Get current swap chain
			ID3D12Resource* d3dBuffer{ buffers[(frameCount - 1) % BUFFER_COUNT].Get() };

			// Barrier Present -> RenderTarget
			D3D12_RESOURCE_BARRIER barrierDesc;
			ZeroMemory(&barrierDesc, sizeof(barrierDesc));
			barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierDesc.Transition.pResource = d3dBuffer;
			barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			commandList->ResourceBarrier(1, &barrierDesc);

			// Viewport
			D3D12_VIEWPORT viewport;
			ZeroMemory(&viewport, sizeof(viewport));
			viewport.Width = (float)windowWidth;
			viewport.Height = (float)windowHeight;
			commandList->RSSetViewports(1, &viewport);

			// Clear
			{
				auto saturate = [](float a) { return a < 0 ? 0 : a > 1 ? 1 : a; };
				float clearColor[4];
				static float h = 0.0f;
				h += 0.01f;
				if (h >= 1) h = 0.0f;
				clearColor[0] = saturate(std::abs(h * 6.0f - 3.0f) - 1.0f);
				clearColor[1] = saturate(2.0f - std::abs(h * 6.0f - 2.0f));
				clearColor[2] = saturate(2.0f - std::abs(h * 6.0f - 4.0f));
				commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
			}

			// Barrier RenderTarget -> Present
			ZeroMemory(&barrierDesc, sizeof(barrierDesc));
			barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierDesc.Transition.pResource = d3dBuffer;
			barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			commandList->ResourceBarrier(1, &barrierDesc);

			// Exec
			hr = commandList->Close();

			ID3D12CommandList* const cmdList = commandList.Get();
			commandQueue->ExecuteCommandLists(1, &cmdList);

			// Present
			hr = swapChain->Present(1, 0);

			// Set queue flushed event
			hr = fence->SetEventOnCompletion(frameCount, fenceEveneHandle);

			// Wait for queue flushed
			// This code would occur CPU stall!
			hr = commandQueue->Signal(fence.Get(), frameCount);

			DWORD wait = WaitForSingleObject(fenceEveneHandle, 10000);
			if (wait != WAIT_OBJECT_0)
			{
				MessageBox(nullptr, "Failed WaitForSingleObject().", "Error", MB_OK);
				return 0;
			}

			// Equivalent code
			/*auto prev = mFence->GetCompletedValue();
			CHK(mCmdQueue->Signal(mFence.Get(), mFrameCount));
			while (prev == mFence->GetCompletedValue())
			{
			}*/

			hr = commandAlloc->Reset();
			hr = commandList->Reset(commandAlloc.Get(), nullptr);
		}
		else
		{
			DispatchMessage(&msg);
		}
	}

	return 0;
}