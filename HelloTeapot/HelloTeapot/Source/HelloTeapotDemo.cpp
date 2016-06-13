#include <stdexcept>
#include <d3dcompiler.h>
#include "..\Headers\HelloTeapotDemo.h"
#include "..\Headers\d3dx12.h"
#include "..\Headers\TeapotData.h"
#include "..\Headers\Window.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

HelloTeapotDemo::HelloTeapotDemo(UINT bufferCount, string name, LONG width, LONG height) : Graphics{ bufferCount, name, width, height }
{
	createControlPointsBuffers();
	createControlPointsIndexBuffer();
	createCbvHeap();
	createConstantBuffers();
	createRootSignature();
	createVertexShader();
	createHullShader();
	createDomainShader();
	createPixelShader();
	createPipelineState();
	createViewport();
	createScissorRect();
}

void HelloTeapotDemo::drawPart(int currPart, int indexView, XMFLOAT3 rot, XMFLOAT3 scale)
{
	UINT frameIndex{ swapChain->GetCurrentBackBufferIndex() };

	ID3D12Resource* constBuffer{ constBuffers[frameIndex].Get() };

	XMMATRIX projMatrixDX{ XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 800.0f / 600.0f, 1.0f, 100.0f) };

	XMVECTOR camPositionDX(XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f));
	XMVECTOR camLookAtDX(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
	XMVECTOR camUpDX(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX viewMatrixDX{ XMMatrixLookAtLH(camPositionDX, camLookAtDX, camUpDX) };

	XMMATRIX viewProjMatrixDX{ viewMatrixDX * projMatrixDX };

	static UINT constDataSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };
	static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

	D3D12_GPU_DESCRIPTOR_HANDLE d{ descHeapCbv->GetGPUDescriptorHandleForHeapStart() };
	d.ptr += (frameIndex * NUM_PARTS + currPart) * descriptorSize;

	commandList->SetGraphicsRootDescriptorTable(0, d);

	POINT mousePoint(window->getMousePosition());
	float pitch{ -XMConvertToRadians((mousePoint.x - (800.0f / 2.0f)) / (800.0f / 2.0f) * 180.0f) };
	float roll{ XMConvertToRadians((mousePoint.y - (600.0f / 2.0f)) / (600.0f / 2.0f) * 180.0f) };

	XMFLOAT4X4 mvpMatrix;
	XMMATRIX modelMatrixDX{ XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z) * XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixRotationRollPitchYaw(roll, pitch, 0.0f) };
	XMStoreFloat4x4(&mvpMatrix, modelMatrixDX * viewProjMatrixDX);

	uint8_t* cbvDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	constBuffer->Map(0, &readRange, reinterpret_cast<void**>(&cbvDataBegin));
	memcpy(&cbvDataBegin[currPart * constDataSizeAligned], &mvpMatrix, sizeof(mvpMatrix));
	constBuffer->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW& view = controlPointsIndexBufferViews[indexView];
	commandList->IASetIndexBuffer(&view);

	uint32_t numIndices{ view.SizeInBytes / sizeof(uint32_t) };
	commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}

void HelloTeapotDemo::render()
{
	UINT frameIndex{ swapChain->GetCurrentBackBufferIndex() };

	ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[frameIndex] };

	if (FAILED(commandAllocator->Reset()))
	{
		throw(runtime_error{ "Error resetting command allocator." });
	}

	if (FAILED(commandList->Reset(commandAllocator.Get(), nullptr)))
	{
		throw(runtime_error{ "Error resetting command list." });
	}

	commandList->SetPipelineState(pipelineState.Get());
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	ID3D12Resource* currBuffer{ swapChainBuffers[frameIndex].Get() };

	D3D12_RESOURCE_BARRIER barrierDesc;
	ZeroMemory(&barrierDesc, sizeof(barrierDesc));
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = currBuffer;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrierDesc);

	UINT descHandleRtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv = descHeapRtv->GetCPUDescriptorHandleForHeapStart();
	descHandleRtv.ptr += frameIndex * descHandleRtvStep;

	commandList->OMSetRenderTargets(1, &descHandleRtv, FALSE, nullptr);

	static float clearColor[]{ 0.0f, 0.5f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

	vector<D3D12_VERTEX_BUFFER_VIEW> myArray{ controlPointsBufferView };
	commandList->IASetVertexBuffers(0, static_cast<UINT>(myArray.size()), myArray.data());

	ID3D12DescriptorHeap* ppHeaps[] = { descHeapCbv.Get() };
	commandList->SetDescriptorHeaps(1, ppHeaps);

	ID3D12Resource* constBuffer{ constBuffers[frameIndex].Get() };

	XMMATRIX projMatrixDX{ XMMatrixPerspectiveFovLH(XMConvertToRadians(45), 800.0f / 600.0f, 1.0f, 100.0f) };

	XMVECTOR camPositionDX(XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f));
	XMVECTOR camLookAtDX(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
	XMVECTOR camUpDX(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX viewMatrixDX{ XMMatrixLookAtLH(camPositionDX, camLookAtDX, camUpDX) };

	XMMATRIX viewProjMatrixDX{ viewMatrixDX * projMatrixDX };

	//******DRAW_TEAPOT*************************************************************************
	int currPart{ 0 };
	// Rim
	drawPart(currPart++, 0, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 0, { 0.0f, 0.0f, XMConvertToRadians(90.0f) });
	drawPart(currPart++, 0, { 0.0f, 0.0f, XMConvertToRadians(180.0f) });
	drawPart(currPart++, 0, { 0.0f, 0.0f, XMConvertToRadians(270.0f) });

	// Body
	drawPart(currPart++, 1, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 1, { 0.0f, 0.0f, XMConvertToRadians(90.0f) });
	drawPart(currPart++, 1, { 0.0f, 0.0f, XMConvertToRadians(180.0f) });
	drawPart(currPart++, 1, { 0.0f, 0.0f, XMConvertToRadians(270.0f) });

	drawPart(currPart++, 2, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 2, { 0.0f, 0.0f, XMConvertToRadians(90.0f) });
	drawPart(currPart++, 2, { 0.0f, 0.0f, XMConvertToRadians(180.0f) });
	drawPart(currPart++, 2, { 0.0f, 0.0f, XMConvertToRadians(270.0f) });

	// Lid
	drawPart(currPart++, 3, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 3, { 0.0f, 0.0f, XMConvertToRadians(90.0f) });
	drawPart(currPart++, 3, { 0.0f, 0.0f, XMConvertToRadians(180.0f) });
	drawPart(currPart++, 3, { 0.0f, 0.0f, XMConvertToRadians(270.0f) });

	drawPart(currPart++, 4, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 4, { 0.0f, 0.0f, XMConvertToRadians(90.0f) });
	drawPart(currPart++, 4, { 0.0f, 0.0f, XMConvertToRadians(180.0f) });
	drawPart(currPart++, 4, { 0.0f, 0.0f, XMConvertToRadians(270.0f) });

	// Handle
	drawPart(currPart++, 5, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 5, { 0.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 1.0f });

	drawPart(currPart++, 6, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 6, { 0.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 1.0f });

	// Spout
	drawPart(currPart++, 7, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 7, { 0.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 1.0f });

	drawPart(currPart++, 8, { 0.0f, 0.0f, 0.0f });
	drawPart(currPart++, 8, { 0.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 1.0f });

	//*******************************************************************************

	ZeroMemory(&barrierDesc, sizeof(barrierDesc));
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = currBuffer;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrierDesc);

	if (FAILED(commandList->Close()))
	{
		throw(runtime_error{ "Failed closing command list." });
	}

	ID3D12CommandList* cmdList{ commandList.Get() };
	commandQueue->ExecuteCommandLists(1, &cmdList);

	if (FAILED(swapChain->Present(1, 0)))
	{
		throw(runtime_error{ "Failed present." });
	}

	UINT64& fenceValue{ fenceValues[frameIndex] };
	++fenceValue;
	ComPtr<ID3D12Fence> fence{ fences[frameIndex] };
	if (FAILED(commandQueue->Signal(fence.Get(), fenceValue)))
	{
		throw(runtime_error{ "Failed signal." });
	}

	waitForPreviousFrame();
}

void HelloTeapotDemo::createControlPointsBuffer()
{
	UINT elementSize{ static_cast<UINT>(sizeof(decltype(TeapotData::points)::value_type)) };

	UINT bufferSize{ static_cast<UINT>(TeapotData::points.size() * elementSize) };

	HRESULT hr{ device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(controlPointsBuffer.ReleaseAndGetAddressOf())) };

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating control points buffer." });
	}

	controlPointsBuffer->SetName(L"Control Points Buffer");

	ComPtr<ID3D12Resource> uploadHeap;
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadHeap.ReleaseAndGetAddressOf()));

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating control points buffer upload heap." });
	}

	uploadHeap->SetName(L"Control Points Buffer Upload");

	D3D12_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pData = TeapotData::points.data();
	data.RowPitch = bufferSize;
	data.SlicePitch = bufferSize;

	ComPtr<ID3D12GraphicsCommandList> commandList{ commandLists[0] };
	ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[0] };
	commandList->Reset(commandAllocator.Get(), nullptr);
	UpdateSubresources(commandList.Get(), controlPointsBuffer.Get(), uploadHeap.Get(), 0, 0, 1, &data);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(controlPointsBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	commandList->Close();
	vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
	commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

	ComPtr<ID3D12Fence> fence{ fences[0] };
	if (FAILED(commandQueue->Signal(fence.Get(), 1)))
	{
		throw(runtime_error{ "Error siganalling control points buffer uploaded." });
	}

	HANDLE fenceEventHandle{ fenceEventHandles[0] };
	if (FAILED(fence->SetEventOnCompletion(1, fenceEventHandle)))
	{
		throw(runtime_error{ "Failed set event on completion." });
	}

	DWORD wait{ WaitForSingleObject(fenceEventHandle, 10000) };
	if (wait != WAIT_OBJECT_0)
	{
		throw(runtime_error{ "Failed WaitForSingleObject()." });
	}

	if (FAILED(fence->Signal(0)))
	{
		throw(runtime_error{ "Error setting fence value." });
	}

	controlPointsBufferView.BufferLocation = controlPointsBuffer->GetGPUVirtualAddress();
	controlPointsBufferView.StrideInBytes = elementSize;
	controlPointsBufferView.SizeInBytes = bufferSize;
}

void HelloTeapotDemo::createControlPointsIndexBuffers()
{
	for (int i{ 0 }; i < TeapotData::patches.size(); ++i)
	{
		auto& patch = TeapotData::patches[i];

		UINT elementSize{ static_cast<UINT>(sizeof(uint32_t)) };

		UINT bufferSize{ static_cast<UINT>(patch.size() * elementSize) };

		Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
		HRESULT hr{ device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())) };

		if (FAILED(hr))
		{
			throw(runtime_error{ "Error creating control points index buffer." });
		}

		controlPointsIndexBuffers.push_back(buffer);

		buffer->SetName(L"Control Points Index Buffer");

		ComPtr<ID3D12Resource> uploadHeap;
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadHeap.ReleaseAndGetAddressOf()));

		if (FAILED(hr))
		{
			throw(runtime_error{ "Error creating control points index buffer upload heap." });
		}

		uploadHeap->SetName(L"Control Points Index Buffer Upload");

		D3D12_SUBRESOURCE_DATA data;
		ZeroMemory(&data, sizeof(data));
		data.pData = patch.data();
		data.RowPitch = bufferSize;
		data.SlicePitch = bufferSize;

		ComPtr<ID3D12GraphicsCommandList> commandList{ commandLists[0] };
		ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[0] };
		commandList->Reset(commandAllocator.Get(), nullptr);
		UpdateSubresources(commandList.Get(), buffer.Get(), uploadHeap.Get(), 0, 0, 1, &data);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		commandList->Close();
		vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
		commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

		ComPtr<ID3D12Fence> fence{ fences[0] };
		if (FAILED(commandQueue->Signal(fence.Get(), 1)))
		{
			throw(runtime_error{ "Error siganalling control points buffer uploaded." });
		}

		HANDLE fenceEventHandle{ fenceEventHandles[0] };
		if (FAILED(fence->SetEventOnCompletion(1, fenceEventHandle)))
		{
			throw(runtime_error{ "Failed set event on completion." });
		}

		DWORD wait{ WaitForSingleObject(fenceEventHandle, 10000) };
		if (wait != WAIT_OBJECT_0)
		{
			throw(runtime_error{ "Failed WaitForSingleObject()." });
		}

		if (FAILED(fence->Signal(0)))
		{
			throw(runtime_error{ "Error setting fence value." });
		}

		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = buffer->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = bufferSize;

		controlPointsIndexBufferViews.push_back(view);
	}
}

void HelloTeapotDemo::createCbvHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.NumDescriptors = NUM_PARTS * bufferCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask = 0;

	if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(descHeapCbv.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating constant buffer heap." });
	}
}

void HelloTeapotDemo::createConstantBuffers()
{
	UINT elementSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };
	UINT64 bufferSize{ elementSizeAligned * NUM_PARTS };

	UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

	for (UINT i{ 0 }; i < bufferCount; ++i)
	{
		ComPtr<ID3D12Resource> constBuffer;
		HRESULT hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(constBuffer.ReleaseAndGetAddressOf()));

		constBuffers.push_back(constBuffer);

		for (int partIndex{ 0 }; partIndex < NUM_PARTS; ++partIndex)
		{
			D3D12_GPU_VIRTUAL_ADDRESS address{ constBuffer->GetGPUVirtualAddress() };
			address += partIndex * elementSizeAligned;

			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			ZeroMemory(&desc, sizeof(desc));
			desc.BufferLocation = address;
			desc.SizeInBytes = elementSizeAligned;	// CB size is required to be 256-byte aligned.

			D3D12_CPU_DESCRIPTOR_HANDLE d{ descHeapCbv->GetCPUDescriptorHandleForHeapStart() };
			d.ptr += (i * NUM_PARTS + partIndex) * descriptorSize;
			device->CreateConstantBufferView(&desc, d);
		}
	}
}

void HelloTeapotDemo::createRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE ranges[1];
	CD3DX12_ROOT_PARAMETER rootParameters[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_DOMAIN);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;


	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(1, rootParameters, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, signature.ReleaseAndGetAddressOf(), error.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error serializing root signature" });
	}

	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating root signature" });
	}
}

void HelloTeapotDemo::createVertexShader()
{
	if (FAILED(D3DReadFileToBlob(L"CompiledShaders/TeapotVertexShader.cso", vertexShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading vertex shader." });
	}
}

void HelloTeapotDemo::createHullShader()
{
	if (FAILED(D3DReadFileToBlob(L"CompiledShaders/TeapotHullShader.cso", hullShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading hull shader." });
	}
}

void HelloTeapotDemo::createDomainShader()
{
	if (FAILED(D3DReadFileToBlob(L"CompiledShaders/TeapotDomainShader.cso", domainShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading domain shader." });
	}
}

void HelloTeapotDemo::createPixelShader()
{
	if (FAILED(D3DReadFileToBlob(L"CompiledShaders/TeapotPixelShader.cso", pixelShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading pixel shader." });
	}
}

void HelloTeapotDemo::createPipelineState()
{
	vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };
	desc.pRootSignature = rootSignature.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	desc.HS = CD3DX12_SHADER_BYTECODE(hullShaderBlob.Get());
	desc.DS = CD3DX12_SHADER_BYTECODE(domainShaderBlob.Get());
	desc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	desc.RasterizerState = rasterizerDesc;// CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;

	if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating pipeline state." });
	}
}

void HelloTeapotDemo::createViewport()
{
	RECT rect;
	if (!GetClientRect(window->getHandle(), &rect))
	{
		throw(runtime_error{ "Error getting window size." });
	}

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<FLOAT>(rect.right - rect.left);
	viewport.Height = static_cast<FLOAT>(rect.bottom - rect.top);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void HelloTeapotDemo::createScissorRect()
{
	RECT rect;
	if (!GetClientRect(window->getHandle(), &rect))
	{
		throw(runtime_error{ "Error getting window size." });
	}

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = rect.right - rect.left;
	scissorRect.bottom = rect.bottom - rect.top;
}