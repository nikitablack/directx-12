#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <vector>
#include <string>

namespace details
{
	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12Resource> createDefaultBuffer(ID3D12Device* device, const std::vector<T>& data, D3D12_RESOURCE_STATES finalState, std::wstring name = L"")
	{
		UINT elementSize{ static_cast<UINT>(sizeof(T)) };
		UINT bufferSize{ static_cast<UINT>(data.size() * elementSize) };

		D3D12_HEAP_PROPERTIES heapProps;
		ZeroMemory(&heapProps, sizeof(heapProps));
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc;
		ZeroMemory(&resourceDesc, sizeof(resourceDesc));
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = bufferSize;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;
		HRESULT hr{ device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.ReleaseAndGetAddressOf())) };

		if (FAILED(hr))
		{
			throw(runtime_error{ "Error creating a default buffer." });
		}

		defaultBuffer->SetName(name.c_str());

		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
		hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			throw(runtime_error{ "Error creating an upload buffer." });
		}

		ComPtr<ID3D12CommandAllocator> commandAllocator;
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating a command allocator." });
		}

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating a command list." });
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc;
		ZeroMemory(&queueDesc, sizeof(queueDesc));
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
		if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating a command queue." });
		}

		void* pData;
		if (FAILED(uploadBuffer->Map(0, NULL, &pData)))
		{
			throw(runtime_error{ "Failed map intermediate resource." });
		}

		memcpy(pData, data.data(), bufferSize);
		uploadBuffer->Unmap(0, NULL);

		commandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, bufferSize);

		D3D12_RESOURCE_BARRIER barrierDesc;
		ZeroMemory(&barrierDesc, sizeof(barrierDesc));
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.Transition.pResource = defaultBuffer.Get();
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrierDesc.Transition.StateAfter = finalState;
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(1, &barrierDesc);

		commandList->Close();
		std::vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
		commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

		UINT64 initialValue{ 0 };
		Microsoft::WRL::ComPtr<ID3D12Fence> fence;
		if (FAILED(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating a fence." });
		}

		HANDLE fenceEventHandle{ CreateEvent(nullptr, FALSE, FALSE, nullptr) };
		if (fenceEventHandle == NULL)
		{
			throw(runtime_error{ "Error creating a fence event." });
		}

		if (FAILED(commandQueue->Signal(fence.Get(), 1)))
		{
			throw(runtime_error{ "Error siganalling buffer uploaded." });
		}

		if (FAILED(fence->SetEventOnCompletion(1, fenceEventHandle)))
		{
			throw(runtime_error{ "Failed set event on completion." });
		}

		DWORD wait{ WaitForSingleObject(fenceEventHandle, 10000) };
		if (wait != WAIT_OBJECT_0)
		{
			throw(runtime_error{ "Failed WaitForSingleObject()." });
		}

		return defaultBuffer;
	}
}

namespace teapot_tutorial
{
	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12Resource> createVertexBuffer(ID3D12Device* device, const std::vector<T>& data, std::wstring name = L"")
	{
		return details::createDefaultBuffer(device, data, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, name);
	}

	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12Resource> createIndexBuffer(ID3D12Device* device, const std::vector<T>& data, std::wstring name = L"")
	{
		return details::createDefaultBuffer(device, data, D3D12_RESOURCE_STATE_INDEX_BUFFER, name);
	}

	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12Resource> createStructuredBuffer(ID3D12Device* device, const std::vector<T>& data, std::wstring name = L"")
	{
		return details::createDefaultBuffer(device, data, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, name);
	}

	template<typename T>
	void createSrv(ID3D12Device* device, ID3D12DescriptorHeap* descHeap, int offset, ID3D12Resource* resource, size_t numElements)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_UNKNOWN; // https://msdn.microsoft.com/en-us/library/windows/desktop/dn859358(v=vs.85).aspx#shader_resource_view
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = static_cast<UINT>(numElements);
		srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(T));
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
		D3D12_CPU_DESCRIPTOR_HANDLE d{ descHeap->GetCPUDescriptorHandleForHeapStart() };
		d.ptr += descriptorSize * offset;
		device->CreateShaderResourceView(resource, &srvDesc, d);
	}
}