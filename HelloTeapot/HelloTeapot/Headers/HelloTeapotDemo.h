#pragma once

#include <DirectXMath.h>
#include "Graphics1.h"

class HelloTeapotDemo : public Graphics1
{
public:
	HelloTeapotDemo(UINT bufferCount, std::string name, LONG width, LONG height);

	void render();

private:
	struct InstanceData
	{
		DirectX::XMFLOAT4X4 modelMatrix;
		XMFLOAT3 color; // should start at 16 bytes aligned offset
	};

private:
	void createInstanceBuffers();
	void createControlPointsBuffer();
	void createControlPointsIndexBuffers();
	void createCbvHeap();
	void createConstantBuffers();
	void createRootSignature();
	void createVertexShader();
	void createHullShader();
	void createDomainShader();
	void createPixelShader();
	void createPipelineState();
	void createViewport();
	void createScissorRect();
	void drawPart(int currPart, int indexView, DirectX::XMFLOAT3 rot, DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });

private:
	const int NUM_PARTS{ 28 };

	Microsoft::WRL::ComPtr<ID3D12Resource> controlPointsBuffer;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> controlPointsIndexBuffers;
	D3D12_VERTEX_BUFFER_VIEW controlPointsBufferView;
	std::vector<D3D12_INDEX_BUFFER_VIEW> controlPointsIndexBufferViews;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeapCbv;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> constBuffers;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> instanceBuffers;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> hullShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> domainShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};