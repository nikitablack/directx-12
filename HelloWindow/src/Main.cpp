#include <Windows.h>
#include <wrl/client.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <vector>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

using namespace std;
using Microsoft::WRL::ComPtr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
};

HWND createWindow(const LONG width, const LONG height)
{
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
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

	HWND hWnd{ CreateWindowEx(0, "WindowClass", "Hello World", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, nullptr, nullptr) };
	if (!hWnd)
	{
		throw(runtime_error{ "Error creating window." });
	}

	ShowWindow(hWnd, SW_SHOW);

	return hWnd;
}

ComPtr<IDXGIFactory4> createFactory()
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT factoryFlags{ 0 };
#if _DEBUG
	factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	HRESULT hr{ CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory)) };
	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating IDXGIFactory2." });
	}

	return dxgiFactory;
}

ComPtr<IDXGIAdapter3> getAdapter(IDXGIFactory4* dxgiFactory)
{
	ComPtr<IDXGIAdapter1> adapterTemp;
	ComPtr<IDXGIAdapter3> adapter;

	for (UINT adapterIndex{ 0 }; dxgiFactory->EnumAdapters1(adapterIndex, adapterTemp.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		ZeroMemory(&desc, sizeof(desc));

		adapterTemp->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		HRESULT hr{ adapterTemp.As(&adapter) };
		if (SUCCEEDED(hr))
		{
			break;
		}
	}

	if (adapter == nullptr)
	{
		throw(runtime_error{ "Error getting hardware adapter." });
	}

	return adapter;
}

ComPtr<ID3D12Device> createDevice(IDXGIAdapter3* adapter)
{
	ComPtr<ID3D12Device> device;
	HRESULT hr{ D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)) };
	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating device." });
	}

	return device;
}

ComPtr<ID3D12CommandQueue> createCommandQueue(ID3D12Device* device)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(queueDesc));
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	ComPtr<ID3D12CommandQueue> commandQueue;
	HRESULT hr{ device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf())) };
	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating command queue." });
	}

	return commandQueue;
}

ComPtr<IDXGISwapChain3> createSwapChain(const LONG width, const LONG height, const UINT bufferCount, IDXGIFactory4* dxgiFactory, ID3D12CommandQueue* commandQueue, HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 }; // no anti-aliasing
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	if (FAILED(dxgiFactory->CreateSwapChainForHwnd(commandQueue, hWnd, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain1." });
	}

	ComPtr<IDXGISwapChain3> swapChain;
	if (FAILED(swapChain1.As(&swapChain)))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain3." });
	}

	return swapChain;
}

vector<ComPtr<ID3D12Resource>> getBuffers(const UINT bufferCount, IDXGISwapChain3* swapChain)
{
	vector<ComPtr<ID3D12Resource>> buffers(bufferCount);
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(buffers[i].ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error getting buffer." });
		}

		buffers[i]->SetName(L"SwapChain_Buffer");
	}

	return buffers;
}

ComPtr<ID3D12DescriptorHeap> createDescriptoprHeapRTV(ID3D12Device* device, const UINT bufferCount, vector<ComPtr<ID3D12Resource>>& buffers)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = 10;
	heapDesc.NodeMask = 0;

	ComPtr<ID3D12DescriptorHeap> descHeapRtv;
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descHeapRtv.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating descriptor heap." });
	}

	UINT rtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE d{ descHeapRtv->GetCPUDescriptorHandleForHeapStart() };
		d.ptr += i * rtvStep;
		device->CreateRenderTargetView(buffers[i].Get(), nullptr, d);
	}

	return descHeapRtv;
}

ComPtr<ID3D12CommandAllocator> createCommandAllocator(ID3D12Device* device)
{
	ComPtr<ID3D12CommandAllocator> commandAlloc;
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAlloc.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating command allocator." });
	}

	return commandAlloc;
}

ComPtr<ID3D12GraphicsCommandList> createCommandList(ID3D12Device* device, ID3D12CommandAllocator* commandAlloc)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;

	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAlloc, nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating command list." });
	}

	return commandList;
}

HANDLE createFenceEvent()
{
	HANDLE fenceEventHandle{ CreateEvent(nullptr, FALSE, FALSE, nullptr) };
	if (fenceEventHandle == NULL)
	{
		throw(runtime_error{ "Error creating fence event." });
	}

	return fenceEventHandle;
}

ComPtr<ID3D12Fence> createFence(ID3D12Device* device)
{
	ComPtr<ID3D12Fence> fence;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating fence." });
	}

	return fence;
}

void render(ID3D12Device* device, ID3D12DescriptorHeap* descHeapRtv, const UINT bufferCount, const LONG width, const LONG height, vector<ComPtr<ID3D12Resource>>& buffers, ID3D12GraphicsCommandList* commandList, ID3D12CommandQueue* commandQueue, IDXGISwapChain3* swapChain, ID3D12Fence* fence, HANDLE fenceEventHandle, ID3D12CommandAllocator* commandAlloc)
{
	static uint64_t frameCount{ 0 };

	++frameCount;

	// Get current RTV descriptor
	UINT descHandleRtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv{ descHeapRtv->GetCPUDescriptorHandleForHeapStart() };
	descHandleRtv.ptr += ((frameCount - 1) % bufferCount) * descHandleRtvStep;

	// Get current swap chain
	ID3D12Resource* d3dBuffer{ buffers[(frameCount - 1) % bufferCount].Get() };

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
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	commandList->RSSetViewports(1, &viewport);

	// Clear
	static float clearColor[]{ 0.0f, 0.5f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);

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
	if (FAILED(commandList->Close()))
	{
		throw(runtime_error{ "Failed closing command list." });
	}

	ID3D12CommandList* cmdList{ commandList };
	commandQueue->ExecuteCommandLists(1, &cmdList);

	// Present
	if (FAILED(swapChain->Present(1, 0)))
	{
		throw(runtime_error{ "Failed present." });
	}

	// Set queue flushed event
	if (FAILED(fence->SetEventOnCompletion(frameCount, fenceEventHandle)))
	{
		throw(runtime_error{ "Failed set event on completion." });
	}

	// Wait for queue flushed
	// This code would occur CPU stall!
	if (FAILED(commandQueue->Signal(fence, frameCount)))
	{
		throw(runtime_error{ "Failed signal." });
	}

	DWORD wait = WaitForSingleObject(fenceEventHandle, 10000);
	if (wait != WAIT_OBJECT_0)
	{
		throw(runtime_error{ "Failed WaitForSingleObject()." });
	}

	if (FAILED(commandAlloc->Reset()))
	{
		throw(runtime_error{ "Failed command allocator reset." });
	}

	if (FAILED(commandList->Reset(commandAlloc, nullptr)))
	{
		throw(runtime_error{ "Failed command list reset." });
	}
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const LONG width{ 800 };
	const LONG height{ 600 };
	const UINT bufferCount{ 2 };

	HWND hWnd;
	ComPtr<IDXGIFactory4> dxgiFactory;
	ComPtr<IDXGIAdapter3> adapter;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<IDXGISwapChain3> swapChain;
	vector<ComPtr<ID3D12Resource>> buffers;
	ComPtr<ID3D12DescriptorHeap> descHeapRtv;
	ComPtr<ID3D12CommandAllocator> commandAlloc;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEventHandle;

	try
	{
		hWnd = createWindow(width, height);
		dxgiFactory = createFactory();
		adapter = getAdapter(dxgiFactory.Get());
		device = createDevice(adapter.Get());
		commandQueue = createCommandQueue(device.Get());
		swapChain = createSwapChain(width, height, bufferCount, dxgiFactory.Get(), commandQueue.Get(), hWnd);
		buffers = getBuffers(bufferCount, swapChain.Get());
		descHeapRtv = createDescriptoprHeapRTV(device.Get(), bufferCount, buffers);
		commandAlloc = createCommandAllocator(device.Get());
		commandList = createCommandList(device.Get(), commandAlloc.Get());
		fence = createFence(device.Get());
		fenceEventHandle = createFenceEvent();
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
				render(device.Get(), descHeapRtv.Get(), bufferCount, width, height, buffers, commandList.Get(), commandQueue.Get(), swapChain.Get(), fence.Get(), fenceEventHandle, commandAlloc.Get());
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