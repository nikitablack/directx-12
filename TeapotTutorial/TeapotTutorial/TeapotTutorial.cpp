#include <stdexcept>
#include <d3dcompiler.h>
#include "TeapotTutorial.h"
//#include "d3dx12.h"
#include "TeapotData.h"
#include "Window.h"
#include "Utils.h"

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

TeapotTutorial::TeapotTutorial(UINT bufferCount, string name, LONG width, LONG height) : Graphics1{ bufferCount, name, width, height }
{
	using PointType = decltype(TeapotData::points)::value_type;
	using TransformType = decltype(TeapotData::patchesTransforms)::value_type;
	using ColorType = decltype(TeapotData::patchesColors)::value_type;

	controlPointsBuffer = teapot_tutorial::createDefaultBuffer(device.Get(), TeapotData::points, L"control points");
	controlPointsIndexBuffer = teapot_tutorial::createDefaultBuffer(device.Get(), TeapotData::patches, L"patches");

	controlPointsBufferView.BufferLocation = controlPointsBuffer->GetGPUVirtualAddress();
	controlPointsBufferView.StrideInBytes = static_cast<UINT>(sizeof(PointType));
	controlPointsBufferView.SizeInBytes = static_cast<UINT>(controlPointsBufferView.StrideInBytes * TeapotData::points.size());

	controlPointsIndexBufferView.BufferLocation = controlPointsIndexBuffer->GetGPUVirtualAddress();
	controlPointsIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	controlPointsIndexBufferView.SizeInBytes = static_cast<UINT>(TeapotData::patches.size() * sizeof(uint32_t));

	transformsBuffer = teapot_tutorial::createDefaultBuffer(device.Get(), TeapotData::patchesTransforms, L"transforms");
	colorsBuffer = teapot_tutorial::createDefaultBuffer(device.Get(), TeapotData::patchesColors, L"colors");

	createTransformsAndColorsDescHeap();
	
	/*transformsDescHeap = */teapot_tutorial::createDescriptorHeapAndSrv<TransformType>(device.Get(), transformsAndColorsDescHeap.Get(), 0, transformsBuffer.Get(), TeapotData::patchesTransforms.size());
	/*colorsDescHeap = */teapot_tutorial::createDescriptorHeapAndSrv<ColorType>(device.Get(), transformsAndColorsDescHeap.Get(), 1, colorsBuffer.Get(), TeapotData::patchesColors.size());

	createConstantBuffer();
	createShaders();
	createRootSignature();
	createPipelineStateWireframe();
	createPipelineStateSolid();
	createViewport();
	createScissorRect();

	auto lambda = [this](WPARAM wParam)
	{
		switch (wParam)
		{
		case 49:
			--tessFactor;
			if (tessFactor < 1) tessFactor = 1;
			break;
		case 50:
			++tessFactor;
			if (tessFactor > 64) tessFactor = 64;
			break;
		case 51:
			currPipelineState = pipelineStateWireframe;
			break;
		case 52:
			currPipelineState = pipelineStateSolid;
			break;
		}
	};
	shared_ptr<function<void(WPARAM)>> onKeyPress = make_shared<function<void(WPARAM)>>(lambda);
	window->addKeyPressCallback(onKeyPress);
}

void TeapotTutorial::render()
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

	commandList->SetPipelineState(currPipelineState.Get());
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

	static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv(descHeapRtv->GetCPUDescriptorHandleForHeapStart());
	descHandleRtv.ptr += frameIndex * descriptorSize;

	D3D12_CPU_DESCRIPTOR_HANDLE descHandleDepthStencil(descHeapDepthStencil->GetCPUDescriptorHandleForHeapStart());

	commandList->OMSetRenderTargets(1, &descHandleRtv, FALSE, &descHandleDepthStencil);

	static float clearColor[]{ 0.0f, 0.5f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(descHeapDepthStencil->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

	vector<D3D12_VERTEX_BUFFER_VIEW> myArray{ controlPointsBufferView };
	commandList->IASetVertexBuffers(0, static_cast<UINT>(myArray.size()), myArray.data());

	vector<int> rootConstants{ tessFactor, tessFactor };
	commandList->SetGraphicsRoot32BitConstants(1, static_cast<UINT>(rootConstants.size()), rootConstants.data(), 0);

	ID3D12DescriptorHeap* ppHeaps[] = { transformsAndColorsDescHeap.Get() };
	commandList->SetDescriptorHeaps(1, ppHeaps);
	D3D12_GPU_DESCRIPTOR_HANDLE d { transformsAndColorsDescHeap->GetGPUDescriptorHandleForHeapStart() };
	d.ptr += 0;
	commandList->SetGraphicsRootDescriptorTable(2, d);

	POINT windowSize(window->getSize());
	float ratio{ static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y) };
	XMMATRIX projMatrixDX{ XMMatrixPerspectiveFovLH(XMConvertToRadians(45), ratio, 1.0f, 100.0f) };

	XMVECTOR camPositionDX(XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f));
	XMVECTOR camLookAtDX(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
	XMVECTOR camUpDX(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX viewMatrixDX{ XMMatrixLookAtLH(camPositionDX, camLookAtDX, camUpDX) };

	XMMATRIX viewProjMatrixDX{ viewMatrixDX * projMatrixDX };
	UINT constDataSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };

	POINT mousePoint(window->getMousePosition());
	float pitch{ -XMConvertToRadians((mousePoint.x - (static_cast<float>(windowSize.x) / 2.0f)) / (static_cast<float>(windowSize.x) / 2.0f) * 180.0f) };
	float roll{ XMConvertToRadians((mousePoint.y - (static_cast<float>(windowSize.y) / 2.0f)) / (static_cast<float>(windowSize.y) / 2.0f) * 180.0f) };

	XMMATRIX modelMatrixRotationDX{ XMMatrixRotationRollPitchYaw(roll, pitch, 0.0f) };
	XMMATRIX modelMatrixTranslationDX{ XMMatrixTranslation(0.0f, -1.0f, 0.0f) };
	XMMATRIX modelMatrixDX{ modelMatrixRotationDX * modelMatrixTranslationDX/*XMMatrixRotationRollPitchYaw(roll, pitch, 0.0f)*/ };
	XMFLOAT4X4 mvpMatrix;
	XMStoreFloat4x4(&mvpMatrix, modelMatrixDX * viewProjMatrixDX);

	D3D12_RANGE readRange = {0, 0};
	uint8_t* cbvDataBegin;
	constBuffer->Map(0, &readRange, reinterpret_cast<void**>(&cbvDataBegin));
	memcpy(&cbvDataBegin[frameIndex * constDataSizeAligned], &mvpMatrix, sizeof(mvpMatrix));
	constBuffer->Unmap(0, nullptr);

	commandList->SetGraphicsRootConstantBufferView(0, constBuffer->GetGPUVirtualAddress() + frameIndex * constDataSizeAligned);

	commandList->IASetIndexBuffer(&controlPointsIndexBufferView);

	uint32_t numIndices{ controlPointsIndexBufferView.SizeInBytes / sizeof(uint32_t) };
	commandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);

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

void TeapotTutorial::createTransformsAndColorsDescHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;

	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(transformsAndColorsDescHeap.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating descriptor heap." });
	}
}

void TeapotTutorial::createConstantBuffer()
{
	UINT elementSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };
	UINT64 bufferSize{ elementSizeAligned * bufferCount };

	D3D12_HEAP_PROPERTIES heapProps;
	ZeroMemory(&heapProps, sizeof(heapProps));
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
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

	HRESULT hr{ device->CreateCommittedResource(
		&heapProps, // CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc, // CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constBuffer.ReleaseAndGetAddressOf())
	) };

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating constant buffer." });
	}

	constBuffer->SetName(L"constants");
}

void TeapotTutorial::createShaders()
{
	if (FAILED(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading vertex shader." });
	}

	if (FAILED(D3DReadFileToBlob(L"HullShader.cso", hullShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading hull shader." });
	}

	if (FAILED(D3DReadFileToBlob(L"DomainShader.cso", domainShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading domain shader." });
	}

	if (FAILED(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading pixel shader." });
	}
}

void TeapotTutorial::createRootSignature()
{
	D3D12_DESCRIPTOR_RANGE dsTransformAndColorSrvRange;
	ZeroMemory(&dsTransformAndColorSrvRange, sizeof(dsTransformAndColorSrvRange));
	dsTransformAndColorSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dsTransformAndColorSrvRange.NumDescriptors = 2;
	dsTransformAndColorSrvRange.BaseShaderRegister = 0;
	dsTransformAndColorSrvRange.RegisterSpace = 0;
	dsTransformAndColorSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // todo

	D3D12_ROOT_PARAMETER dsTransformAndColorSrv;
	ZeroMemory(&dsTransformAndColorSrv, sizeof(dsTransformAndColorSrv));
	dsTransformAndColorSrv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	dsTransformAndColorSrv.DescriptorTable = { 1, &dsTransformAndColorSrvRange };
	dsTransformAndColorSrv.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	D3D12_ROOT_PARAMETER dsObjCb;
	ZeroMemory(&dsObjCb, sizeof(dsObjCb));
	dsObjCb.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	dsObjCb.Descriptor = { 0, 0 };
	dsObjCb.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	D3D12_ROOT_PARAMETER hsTessFactorsCb;
	ZeroMemory(&hsTessFactorsCb, sizeof(hsTessFactorsCb));
	hsTessFactorsCb.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	hsTessFactorsCb.Constants = { 0, 0, 2 };
	hsTessFactorsCb.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;

	vector<D3D12_ROOT_PARAMETER> rootParameters{ dsObjCb, hsTessFactorsCb, dsTransformAndColorSrv };

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
	};

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	ZeroMemory(&rootSignatureDesc, sizeof(rootSignatureDesc));
	rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
	rootSignatureDesc.pParameters = rootParameters.data();
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = rootSignatureFlags;

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.ReleaseAndGetAddressOf(), error.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error serializing root signature" });
	}

	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating root signature" });
	}
}

void TeapotTutorial::createPipelineStateWireframe()
{
	pipelineStateWireframe = createPipelineState(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE);
	currPipelineState = pipelineStateWireframe;
}

void TeapotTutorial::createPipelineStateSolid()
{
	pipelineStateSolid = createPipelineState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE);
	//currPipelineState = pipelineStateSolid;
}

ComPtr<ID3D12PipelineState> TeapotTutorial::createPipelineState(D3D12_FILL_MODE fillMode, D3D12_CULL_MODE cullMode)
{
	vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
	rasterizerDesc.FillMode = fillMode;
	rasterizerDesc.CullMode = cullMode;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0] = {
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL
	};

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	depthStencilDesc.FrontFace = defaultStencilOp;
	depthStencilDesc.BackFace = defaultStencilOp;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc;
	ZeroMemory(&pipelineStateDesc, sizeof(pipelineStateDesc));
	pipelineStateDesc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };
	pipelineStateDesc.pRootSignature = rootSignature.Get();
	pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	pipelineStateDesc.HS = { hullShaderBlob->GetBufferPointer(), hullShaderBlob->GetBufferSize() };
	pipelineStateDesc.DS = { domainShaderBlob->GetBufferPointer(), domainShaderBlob->GetBufferSize() };
	pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	pipelineStateDesc.RasterizerState = rasterizerDesc;
	pipelineStateDesc.BlendState = blendDesc;
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.SampleMask = UINT_MAX;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pipelineState;
	if (FAILED(device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating pipeline state." });
	}

	return pipelineState;
}

void TeapotTutorial::createViewport()
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

void TeapotTutorial::createScissorRect()
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