#include "..\Headers\Graphics.h"
#include <stdexcept>
#include "..\Headers\Window.h"

using namespace std;
using namespace Microsoft::WRL;

Graphics::Graphics(UINT bufferCount, string name, LONG width, LONG height) : bufferCount{ bufferCount }, swapChainBuffers(bufferCount)
{
	createFactory(width, height);
	getAdapter(false);
	createDevice();
	createCommandQueue();
	createSwapChain();
	getSwapChainBuffers();
	createDescriptoprHeapRtv();
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

void Graphics::getAdapter(bool useWarp)
{
	ComPtr<IDXGIAdapter1> adapterTemp;

	if (useWarp)
	{
		HRESULT hr{ factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())) };
	}
	else
	{
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
	}

	if (adapter == nullptr)
	{
		throw(runtime_error{ "Error getting an adapter." });
	}
}

void Graphics::createDevice()
{
	HRESULT hr{ D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)) };
	if (FAILED(hr))
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
	swapChainDesc.SampleDesc = { 1, 0 }; // no anti-aliasing
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

/*ID3D12Device* Graphics::getDevice()
{
	return device.Get();
}

ComPtr<ID3D12Device> Graphics::getDeviceCom()
{
	return device;
}

IDXGISwapChain3* Graphics::getSwapChain()
{
	return swapChain.Get();
}*/