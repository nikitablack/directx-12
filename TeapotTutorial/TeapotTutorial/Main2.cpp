#include "Window.h"
#include <wrl/client.h>
#include <memory>
#include <DirectXMath.h>
#include <stdexcept>
#include <array>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <vector>
#include "TeapotData.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "D3DCompiler.lib")

using namespace std;
using namespace Microsoft::WRL;
using namespace DirectX;

const UINT width{ 800 };
const UINT height{ 600 };
const int numParts{ 28 };
const int numBackBuffers{ 2 };

ComPtr<ID3D12Device> device;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<IDXGISwapChain3> swapChain;
array<ComPtr<ID3D12Resource>, numBackBuffers> swapChainBuffers;
ComPtr<ID3D12DescriptorHeap> descHeapRtv;
ComPtr<ID3DBlob> vertexShaderBlob; // try to release after pso creation
ComPtr<ID3DBlob> hullShaderBlob;
ComPtr<ID3DBlob> domainShaderBlob;
ComPtr<ID3DBlob> pixelShaderBlob;
ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12PipelineState> pipelineStateWireframe;
ComPtr<ID3D12PipelineState> pipelineStateSolid;
ComPtr<ID3D12PipelineState> currPipelineState;
array<ComPtr<ID3D12CommandAllocator>, numBackBuffers> commandAllocators;
ComPtr<ID3D12GraphicsCommandList> commandList;
array<ComPtr<ID3D12Fence>, numBackBuffers> fences;
array<UINT64, numBackBuffers> fenceValues;
HANDLE fenceEventHandle;
D3D12_VIEWPORT viewport;
D3D12_RECT scissorRect;
ComPtr<ID3D12Resource> controlPointsBuffer;
ComPtr<ID3D12Resource> controlPointsIndexBuffer;
D3D12_VERTEX_BUFFER_VIEW controlPointsBufferView;
D3D12_INDEX_BUFFER_VIEW controlPointsIndexBufferView;
ComPtr<ID3D12DescriptorHeap> descHeapCbv;
ComPtr<ID3D12Resource> dsConstantBuffer;
ComPtr<ID3D12Resource> dsTransformsStructuredBuffer;
ComPtr<ID3D12Resource> dsColorsStructuredBuffer;

void initialize(HWND);
void createVertexShader();
void createHullShader();
void createDomainShader();
void createPixelShader();
void createRootSignature();
void createPipelineStateWireframe();
void createPipelineStateSolid();
ComPtr<ID3D12PipelineState> createPipelineState(D3D12_FILL_MODE);
void createCommandAllocators();
void createCommandList();
void createFences();
void createFenceEventHandle();
void createViewport();
void createScissorRect();
void createControlPointsBuffer();
void createControlPointsIndexBuffer();
void createDsConstantBuffers();
void createDsTransformsStructuredBuffer();
void createDsColorsStructuredBuffer();
void waitForPreviousFrame();
void render();

int WINAPI WinMain2(HINSTANCE, HINSTANCE, LPSTR, int)
{
	shared_ptr<Window> window;

	try
	{
		window = make_shared<Window>(width, height, "Hello, Teapot!");

		initialize(window->getHandle());
		createVertexShader();
		createHullShader();
		createDomainShader();
		createPixelShader();
		createRootSignature();
		createPipelineStateWireframe();
		createPipelineStateSolid();
		createCommandAllocators();
		createCommandList();
		createFences();
		createFenceEventHandle();
		createControlPointsBuffer();
		createControlPointsIndexBuffer();
		createDsConstantBuffers();
		//createDsStructuredBuffer();
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
				render();
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

void initialize(HWND hWnd)
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

	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating IDXGIFactory." });
	}

	ComPtr<IDXGIAdapter3> adapter;
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

	if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		throw(runtime_error{ "Error creating device." });
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc;
	ZeroMemory(&queueDesc, sizeof(queueDesc));
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 0;

	if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating command queue." });
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 }; // no anti-aliasing
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = numBackBuffers;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain1." });
	}

	if (FAILED(swapChain1.As(&swapChain)))
	{
		throw(runtime_error{ "Error creating IDXGISwapChain3." });
	}

	for (UINT i{ 0 }; i < numBackBuffers; i++)
	{
		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(swapChainBuffers[i].ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error getting buffer." });
		}
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	ZeroMemory(&heapDesc, sizeof(heapDesc));
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = numBackBuffers;
	heapDesc.NodeMask = 0;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(descHeapRtv.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating descriptor heap." });
	}

	UINT rtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	for (UINT i{ 0 }; i < numBackBuffers; i++)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE d = descHeapRtv->GetCPUDescriptorHandleForHeapStart();
		d.ptr += i * rtvStep;
		device->CreateRenderTargetView(swapChainBuffers[i].Get(), nullptr, d);
	}
}

void createVertexShader()
{
	if (FAILED(D3DReadFileToBlob(L"VertexShader.cso", vertexShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading vertex shader." });
	}
}

void createHullShader()
{
	if (FAILED(D3DReadFileToBlob(L"HullShader.cso", hullShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading hull shader." });
	}
}

void createDomainShader()
{
	if (FAILED(D3DReadFileToBlob(L"DomainShader.cso", domainShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading domain shader." });
	}
}

void createPixelShader()
{
	if (FAILED(D3DReadFileToBlob(L"PixelShader.cso", pixelShaderBlob.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error reading pixel shader." });
	}
}

void createRootSignature()
{
	D3D12_DESCRIPTOR_RANGE dsObjCbRange;
	ZeroMemory(&dsObjCbRange, sizeof(dsObjCbRange));
	dsObjCbRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	dsObjCbRange.NumDescriptors = 1;
	dsObjCbRange.BaseShaderRegister = 0;
	dsObjCbRange.RegisterSpace = 0;
	dsObjCbRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // todo

	D3D12_ROOT_PARAMETER dsObjCb;
	ZeroMemory(&dsObjCb, sizeof(dsObjCb));
	dsObjCb.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	dsObjCb.DescriptorTable = { 1, &dsObjCbRange };
	dsObjCb.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	D3D12_ROOT_PARAMETER hsTessFactorsCb;
	ZeroMemory(&hsTessFactorsCb, sizeof(hsTessFactorsCb));
	hsTessFactorsCb.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	hsTessFactorsCb.Constants = { 0, 0, 2 };
	hsTessFactorsCb.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;

	D3D12_ROOT_PARAMETER dsTransformsSrv;
	ZeroMemory(&dsTransformsSrv, sizeof(dsTransformsSrv));
	dsTransformsSrv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	dsTransformsSrv.Descriptor = { 0, 0 };
	dsTransformsSrv.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	D3D12_ROOT_PARAMETER dsColorsSrv;
	ZeroMemory(&dsColorsSrv, sizeof(dsColorsSrv));
	dsColorsSrv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	dsColorsSrv.Descriptor = { 1, 0 };
	dsColorsSrv.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	vector<D3D12_ROOT_PARAMETER> rootParameters{ dsObjCb, hsTessFactorsCb, dsTransformsSrv, dsColorsSrv };

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

void createPipelineStateWireframe()
{
	pipelineStateWireframe = createPipelineState(D3D12_FILL_MODE_WIREFRAME);
	currPipelineState = pipelineStateWireframe;
}

void createPipelineStateSolid()
{
	pipelineStateSolid = createPipelineState(D3D12_FILL_MODE_SOLID);
}

ComPtr<ID3D12PipelineState> createPipelineState(D3D12_FILL_MODE fillMode)
{
	array<D3D12_INPUT_ELEMENT_DESC, 1> inputElementDescs
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
	rasterizerDesc.FillMode = fillMode;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
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
	pipelineStateDesc.DepthStencilState.DepthEnable = FALSE;
	pipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
	pipelineStateDesc.SampleMask = UINT_MAX;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pipelineState;
	if (FAILED(device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating pipeline state." });
	}

	return pipelineState;
}

void createCommandAllocators()
{
	for (UINT i{ 0 }; i < numBackBuffers; i++)
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating command allocator." });
		}

		commandAllocators[i] = commandAllocator;
	}
}

void createCommandList()
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

void createFences()
{
	for (UINT i{ 0 }; i < numBackBuffers; i++)
	{
		UINT64 initialValue{ 0 };
		ComPtr<ID3D12Fence> fence;
		if (FAILED(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
		{
			throw(runtime_error{ "Error creating fence." });
		}

		fences[i] = fence;
		fenceValues[i] = initialValue;
	}
}

void createFenceEventHandle()
{
	fenceEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEventHandle == NULL)
	{
		throw(runtime_error{ "Error creating fence event." });
	}
}

void createViewport()
{
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void createScissorRect()
{
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = width;
	scissorRect.bottom = height;
}

void createControlPointsBuffer()
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

	D3D12_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pData = TeapotData::points.data();
	data.RowPitch = bufferSize;
	data.SlicePitch = bufferSize;

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

void createControlPointsIndexBuffer()
{
	UINT elementSize{ static_cast<UINT>(sizeof(decltype(TeapotData::patches)::value_type)) };
	UINT bufferSize{ static_cast<UINT>(TeapotData::patches.size() * elementSize) };

	HRESULT hr{ device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(controlPointsIndexBuffer.ReleaseAndGetAddressOf())) };

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating control points index buffer." });
	}

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

	D3D12_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pData = TeapotData::patches.data();
	data.RowPitch = bufferSize;
	data.SlicePitch = bufferSize;

	ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[0] };
	commandList->Reset(commandAllocator.Get(), nullptr);
	UpdateSubresources(commandList.Get(), controlPointsIndexBuffer.Get(), uploadHeap.Get(), 0, 0, 1, &data);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(controlPointsIndexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	commandList->Close();
	vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
	commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

	ComPtr<ID3D12Fence> fence{ fences[0] };
	if (FAILED(commandQueue->Signal(fence.Get(), 1)))
	{
		throw(runtime_error{ "Error siganalling control points buffer uploaded." });
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

	if (FAILED(fence->Signal(0)))
	{
		throw(runtime_error{ "Error setting fence value." });
	}

	controlPointsIndexBufferView.BufferLocation = controlPointsIndexBuffer->GetGPUVirtualAddress();
	controlPointsIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	controlPointsIndexBufferView.SizeInBytes = bufferSize;
}

void createDsConstantBuffers()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbHeapDesc;
	ZeroMemory(&cbHeapDesc, sizeof(cbHeapDesc));
	cbHeapDesc.NumDescriptors = numParts * numBackBuffers;
	cbHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbHeapDesc.NodeMask = 0;

	if (FAILED(device->CreateDescriptorHeap(&cbHeapDesc, IID_PPV_ARGS(descHeapCbv.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating constant buffer heap." });
	}

	UINT elementSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };
	UINT64 bufferSize{ elementSizeAligned * numParts * numBackBuffers };

	UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

	HRESULT hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(dsConstantBuffer.ReleaseAndGetAddressOf())
	);

	for (int i{ 0 }; i < numParts * numBackBuffers; ++i)
	{
		D3D12_GPU_VIRTUAL_ADDRESS address{ dsConstantBuffer->GetGPUVirtualAddress() };
		address += i * elementSizeAligned;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		ZeroMemory(&cbvDesc, sizeof(cbvDesc));
		cbvDesc.BufferLocation = address;
		cbvDesc.SizeInBytes = elementSizeAligned;	// CB size is required to be 256-byte aligned.

		D3D12_CPU_DESCRIPTOR_HANDLE d{ descHeapCbv->GetCPUDescriptorHandleForHeapStart() };
		d.ptr += i * descriptorSize;
		device->CreateConstantBufferView(&cbvDesc, d);
	}
}

void createDsColorsStructuredBuffer()
{
	UINT elementSize{ static_cast<UINT>(sizeof(decltype(TeapotData::patchesColors)::value_type)) };
	UINT bufferSize{ static_cast<UINT>(TeapotData::patchesColors.size() * elementSize) };

	HRESULT hr{ device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(dsColorsStructuredBuffer.ReleaseAndGetAddressOf())) };

	if (FAILED(hr))
	{
		throw(runtime_error{ "Error creating color buffer heap." });
	}

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
		throw(runtime_error{ "Error creating color buffer upload heap." });
	}

	D3D12_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pData = TeapotData::patchesColors.data();
	data.RowPitch = bufferSize;
	data.SlicePitch = bufferSize;

	ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[0] };
	commandList->Reset(commandAllocator.Get(), nullptr);
	UpdateSubresources(commandList.Get(), dsColorsStructuredBuffer.Get(), uploadHeap.Get(), 0, 0, 1, &data);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dsColorsStructuredBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	commandList->Close();
	vector<ID3D12CommandList*> ppCommandLists{ commandList.Get() };
	commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()), ppCommandLists.data());

	ComPtr<ID3D12Fence> fence{ fences[0] };
	if (FAILED(commandQueue->Signal(fence.Get(), 1)))
	{
		throw(runtime_error{ "Error siganalling control points buffer uploaded." });
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

	if (FAILED(fence->Signal(0)))
	{
		throw(runtime_error{ "Error setting fence value." });
	}
}

void waitForPreviousFrame()
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

void render()
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

	UINT descHandleRtvStep{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv = descHeapRtv->GetCPUDescriptorHandleForHeapStart();
	descHandleRtv.ptr += frameIndex * descHandleRtvStep;

	commandList->OMSetRenderTargets(1, &descHandleRtv, FALSE, nullptr);

	static float clearColor[]{ 0.0f, 0.5f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

	vector<D3D12_VERTEX_BUFFER_VIEW> controlPointsViews{ controlPointsBufferView };
	commandList->IASetVertexBuffers(0, static_cast<UINT>(controlPointsViews.size()), controlPointsViews.data());

	vector<ID3D12DescriptorHeap*> cbDescHeaps{ descHeapCbv.Get() };
	commandList->SetDescriptorHeaps(static_cast<UINT>(cbDescHeaps.size()), cbDescHeaps.data());

	static UINT constDataSizeAligned{ (sizeof(XMFLOAT4X4) + 255) & ~255 };
	static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

	D3D12_GPU_DESCRIPTOR_HANDLE d{ descHeapCbv->GetGPUDescriptorHandleForHeapStart() };
	d.ptr += (frameIndex * numParts) * descriptorSize;
	commandList->SetGraphicsRootDescriptorTable(0, d);

	vector<int32_t> rootConstants{ 16, 16 };
	commandList->SetGraphicsRoot32BitConstants(1, static_cast<UINT>(rootConstants.size()), rootConstants.data(), 0);

	commandList->SetGraphicsRootShaderResourceView(2, dsColorsStructuredBuffer->GetGPUVirtualAddress());

	XMMATRIX projMatrixDX{ XMMatrixPerspectiveFovLH(XMConvertToRadians(45), 800.0f / 600.0f, 1.0f, 100.0f) };

	XMVECTOR camPositionDX(XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f));
	XMVECTOR camLookAtDX(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));
	XMVECTOR camUpDX(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMMATRIX viewMatrixDX{ XMMatrixLookAtLH(camPositionDX, camLookAtDX, camUpDX) };

	XMMATRIX viewProjMatrixDX{ viewMatrixDX * projMatrixDX };

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