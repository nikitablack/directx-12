#include <wrl/client.h>
#include <memory>
#include <stdexcept>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "D3DCompiler.lib")

using namespace std;
using namespace Microsoft::WRL;

ComPtr<ID3D12RootSignature> rootSignature;
ComPtr<ID3D12Device> device;
ComPtr<IDXGIAdapter3> adapter;
ComPtr<IDXGIFactory4> factory;

void createRootSignature();
void createDevice();
void getAdapter();
void createFactory();

int WINAPI WinMain2(HINSTANCE, HINSTANCE, LPSTR, int)
{
	createFactory();
	getAdapter();
	createDevice();
	createRootSignature();

	return 0;
}

void createRootSignature()
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

void createDevice()
{
	if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		throw(runtime_error{ "Error creating device." });
	}
}

void getAdapter()
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

void createFactory()
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