#pragma once

#include "Graphics.h"
#include <string>
#include <DirectXMath.h>

class HelloTeapotDemo : public Graphics
{
public:
	HelloTeapotDemo(std::shared_ptr<Window> window, UINT bufferCount);

	void render() override;

private:
	void createCommandAllocators();
	void createCommandLists();
	void createFences();
	void createFenceEvents();
	void waitForPreviousFrame();
	void createControlPointsBuffers();
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
	void drawPart(int currPart, int indexView, ID3D12GraphicsCommandList* commandList, DirectX::XMFLOAT3 rot, DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });

private:
	const int NUM_PARTS{ 28 };
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators;
	std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> commandLists;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> fences;
	std::vector<UINT64> fenceValues;
	std::vector<HANDLE> fenceEventHandles;
	Microsoft::WRL::ComPtr<ID3D12Resource> controlPointsBuffer;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> controlPointsIndexBuffers;
	D3D12_VERTEX_BUFFER_VIEW controlPointsBufferView;
	std::vector<D3D12_INDEX_BUFFER_VIEW> controlPointsIndexBufferViews;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeapCbv;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> constBuffers;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> hullShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> domainShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
};