#pragma once

#include <DirectXMath.h>
#include "Graphics1.h"

class TeapotTutorial : public Graphics1
{
public:
	TeapotTutorial(UINT bufferCount, std::string name, LONG width, LONG height);

	void render();

private:
	void createTransformsAndColorsDescHeap();
	void createConstantBuffer();
	void createShaders();
	void createRootSignature();
	void createPipelineStateWireframe();
	void createPipelineStateSolid();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> createPipelineState(D3D12_FILL_MODE fillMode, D3D12_CULL_MODE cullMode);
	void createViewport();
	void createScissorRect();

private:
	const int numParts{ 28 };

	Microsoft::WRL::ComPtr<ID3D12Resource> controlPointsBuffer;
	D3D12_VERTEX_BUFFER_VIEW controlPointsBufferView;
	Microsoft::WRL::ComPtr<ID3D12Resource> controlPointsIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW controlPointsIndexBufferView;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformsBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> colorsBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> transformsAndColorsDescHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> hullShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> domainShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateWireframe;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateSolid;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> currPipelineState;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	int tessFactor{ 8 };
};