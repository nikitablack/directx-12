#include "Graphics.h"
#include <stdexcept>
#include "Window.h"

using namespace std;
using namespace Microsoft::WRL;

Graphics::Graphics(UINT bufferCount, string name, LONG width, LONG height) : bufferCount{ bufferCount }, swapChainBuffers(bufferCount)
{
	createWindow(name, width, height);
	createFactory();
	getAdapter();
	createDevice();
	createCommandQueue();
	createSwapChain();
	getSwapChainBuffers();
	createDescriptoprHeapRtv();
	createDepthStencilBuffer();
	createDescriptorHeapDepthStencil();
	createCommandAllocators();
	createCommandList();
	createFences();
	createFenceEventHandle();
}

Graphics::~Graphics()
{
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		waitFrameComplete(i);
	}
}

void Graphics::createWindow(string name, LONG width, LONG height)
{
	window = make_shared<Window>(width, height, name.c_str());
}

void Graphics::createFactory()
{
#if defined(_DEBUG) 
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	UINT factoryFlags{ 0 };
#if _DEBUG
	factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating IDXGIFactory." });
	}
}

void Graphics::getAdapter()
{
	ComPtr<IDXGIAdapter1> adapterTemp;

	for (UINT adapterIndex{ 0 }; factory->EnumAdapters1(adapterIndex, adapterTemp.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		ZeroMemory(&desc, sizeof(desc));

		adapterTemp->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(adapterTemp.As(&adapter)))
		{
			break;
		}
	}

	if (adapter == nullptr)
	{
		throw(runtime_error{ "Error getting an adapter." });
	}
}

void Graphics::createDevice()
{
	if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		throw(runtime_error{ "Error creating device." });
	}
}

void Graphics::createCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(queueDesc));
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	HRESULT hr{ device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf())) };
	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating command queue." });
	}
}

void Graphics::createSwapChain()
{
	POINT wSize(window->getSize());

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = static_cast<UINT>(wSize.x);
	swapChainDesc.Height = static_cast<UINT>(wSize.y);
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), window->getHandle(), &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain1." });
	}

	if (FAILED(swapChain1.As(&swapChain)))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain3." });
	}
}

void Graphics::getSwapChainBuffers()
{
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(swapChainBuffers[i].ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error getting buffer." });
		}
	}
}

void Graphics::createDescriptoprHeapRtv()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = bufferCount;
	heapDesc.NodeMask = 0;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descHeapRtv.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating descriptor heap." });
	}

	UINT rtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE d = descHeapRtv->GetCPUDescriptorHandleForHeapStart();
		d.ptr += i * rtvStep;
		device->CreateRenderTargetView(swapChainBuffers[i].Get(), nullptr, d);
	}
}

void Graphics::createDepthStencilBuffer()
{
	D3D12_CLEAR_VALUE depthOptimizedClearValue;
	ZeroMemory(&depthOptimizedClearValue, sizeof(depthOptimizedClearValue));
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	POINT wSize(window->getSize());

	D3D12_HEAP_PROPERTIES heapProps;
	ZeroMemory(&heapProps, sizeof(heapProps));
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = wSize.x;
	resourceDesc.Height = wSize.y;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 0;
	resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	HRESULT hr{ device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(depthStencilBuffer.ReleaseAndGetAddressOf())
	) };

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating depth stencil buffer." });
	}
}

void Graphics::createDescriptorHeapDepthStencil()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descHeapDepthStencil.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating depth stencil descriptor heap." });
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
	depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

	device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, descHeapDepthStencil->GetCPUDescriptorHandleForHeapStart());
}

void Graphics::createCommandAllocators()
{
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating command allocator." });
		}

		commandAllocators.push_back(commandAllocator);
	}
}

void Graphics::createCommandList()
{
	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating command list." });
	}

	if (FAILED(commandList->Close()))
	{
		throw(runtime_error{ "Error closing command list." });
	}
}

void Graphics::createFences()
{
	for (UINT i{ 0 }; i < bufferCount; i++)
	{
		UINT64 initialValue{ 0 };
		ComPtr<ID3D12Fence> fence;
		if (FAILED(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating fence." });
		}

		fences.push_back(fence);
		fenceValues.push_back(initialValue);
	}
}

void Graphics::createFenceEventHandle()
{
	fenceEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEventHandle == NULL)
	{
		throw(runtime_error{ "Error creating fence event." });
	}
}

void Graphics::waitFrameComplete(UINT frameIndex)
{
	UINT64 fenceValue{ fenceValues[frameIndex] };
	ComPtr<ID3D12Fence> fence{ fences[frameIndex] };

	if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEventHandle)))
	{
		throw(runtime_error{ "Failed set event on completion." });
	}

	DWORD wait{ WaitForSingleObject(fenceEventHandle, 10000) };
	if (wait != WAIT_OBJECT_0)
	{
		throw(runtime_error{ "Failed WaitForSingleObject()." });
	}
}