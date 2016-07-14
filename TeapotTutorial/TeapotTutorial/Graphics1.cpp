#include "Graphics1.h"
#include <stdexcept>
#include "Window.h"

using namespace std;
using namespace Microsoft::WRL;

Graphics1::Graphics1(UINT bufferCount, string name, LONG width, LONG height) : Graphics{ bufferCount, name, width, height }
{
	createCommandAllocators();
	createCommandList();
	createFences();
	createFenceEventHandle();
}

void Graphics1::createCommandAllocators()
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

void Graphics1::createCommandList()
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

void Graphics1::createFences()
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

void Graphics1::createFenceEventHandle()
{
	fenceEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEventHandle == NULL)
	{
		throw(runtime_error{ "Error creating fence event." });
	}
}

void Graphics1::waitForPreviousFrame()
{
	UINT frameIndex{ swapChain->GetCurrentBackBufferIndex() };
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