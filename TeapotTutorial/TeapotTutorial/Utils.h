#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <vector>
#include <string>
//#include "d3dx12.h"

namespace teapot_tutorial
{
	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12Resource> createDefaultBuffer(ID3D12Device* device, const std::vector<T>& data, std::wstring name = L"")
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
			&heapProps, // CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc, // CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
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
			&heapProps, // CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc, // CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			throw(runtime_error{ "Error creating an upload buffer." });
		}

		D3D12_SUBRESOURCE_DATA subresourceData;
		ZeroMemory(&subresourceData, sizeof(subresourceData));
		subresourceData.pData = data.data();
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = bufferSize;

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

		if (FAILED(commandList->Close()))
		{
			throw(runtime_error{ "Error closing a command list." });
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

		commandList->Reset(commandAllocator.Get(), nullptr);
		UpdateSubresources(commandList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), subresourceData);

		// CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
		D3D12_RESOURCE_BARRIER barrierDesc;
		ZeroMemory(&barrierDesc, sizeof(barrierDesc));
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.Transition.pResource = defaultBuffer.Get();
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		commandList->ResourceBarrier(1, &barrierDesc);

		commandList->Close();
		std::vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
		commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

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

	template<typename T>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeapAndSrv(ID3D12Device* device, ID3D12DescriptorHeap* descHeap, int offset, ID3D12Resource* resource, size_t numElements)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc; // https://msdn.microsoft.com/en-us/library/windows/desktop/dn770406(v=vs.85).aspx
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

		return descHeap;
	}

	UINT64 UpdateSubresources2(
		_In_ ID3D12GraphicsCommandList* pCmdList,
		_In_ ID3D12Resource* pDestinationResource,
		_In_ ID3D12Resource* pIntermediate,
		UINT64 RequiredSize,
		_In_reads_(1) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint,
		_In_reads_(1) const UINT numRows,
		_In_reads_(1) const UINT64 rowSizesInBytes,
		_In_reads_(1) const D3D12_SUBRESOURCE_DATA srcData)
	{
		pDestinationResource->GetDesc();
		BYTE* pData;
		HRESULT hr{ pIntermediate->Map(0, NULL, reinterpret_cast<void**>(&pData)) };
		if (FAILED(hr))
		{
			throw(std::runtime_error{ "Failed map intermediate resource." });
		}

		if (rowSizesInBytes >(SIZE_T)-1) return 0;
		pDestinationResource->GetDesc();
		D3D12_MEMCPY_DEST DestData = { pData + footprint.Offset, footprint.Footprint.RowPitch, footprint.Footprint.RowPitch * numRows };
		//MemcpySubresource(&DestData, &srcData, (SIZE_T)rowSizesInBytes, numRows, footprint.Footprint.Depth);
		memcpy(pData, srcData.pData, rowSizesInBytes);
		pIntermediate->Unmap(0, NULL);

		
		//CD3DX12_BOX SrcBox(UINT(footprint.Offset), UINT(footprint.Offset + footprint.Footprint.Width));
		pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate, footprint.Offset, footprint.Footprint.Width);

		return RequiredSize;
	}

	UINT64 UpdateSubresources(
		_In_ ID3D12GraphicsCommandList* pCmdList,
		_In_ ID3D12Resource* pDestinationResource,
		_In_ ID3D12Resource* pIntermediate,
		_In_reads_(1) D3D12_SUBRESOURCE_DATA srcData)
	{
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint; // 0, DXGI_FORMAT_UNKNOWN, 1416, 1, 1, 1536
		UINT numRows; // ? 1
		UINT64 rowSizesInBytes; // ? 1416
		UINT64 RequiredSize; // 1416

		D3D12_RESOURCE_DESC Desc = pDestinationResource->GetDesc();
		ID3D12Device* pDevice;
		pDestinationResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
		pDevice->GetCopyableFootprints(&Desc, 0, 1, 0, &footprint, &numRows, &rowSizesInBytes, &RequiredSize);
		pDevice->Release();
		
		UINT64 Result = UpdateSubresources2(pCmdList, pDestinationResource, pIntermediate, RequiredSize, footprint, numRows, rowSizesInBytes, srcData);
		
		return Result;
	}
}