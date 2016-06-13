#pragma once

#include "Graphics.h"

class Graphics1 : Graphics
{
public:
	Graphics1(UINT bufferCount, std::string name, LONG width, LONG height);

private:
	void createCommandAllocators();
	void createCommandList();
	void createFences();
	void createFenceEventHandle();
	void waitForPreviousFrame();

protected:
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> commandAllocators;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Fence>> fences;
	std::vector<UINT64> fenceValues;
	HANDLE fenceEventHandle;
};