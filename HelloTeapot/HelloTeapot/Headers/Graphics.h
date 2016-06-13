#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>

class Graphics
{
public:
	Graphics(UINT bufferCount, std::string name, LONG width, LONG height);

	//virtual void render() = 0;

	/*ID3D12Device* getDevice();
	Microsoft::WRL::ComPtr<ID3D12Device> getDeviceCom();
	IDXGISwapChain3* getSwapChain();*/

private:
	void createWindow(std::string name, LONG width, LONG height);
	void createFactory();
	void getAdapter(bool useWarp);
	void createDevice();
	void createCommandQueue();
	void createSwapChain();
	void getSwapChainBuffers();
	void createDescriptoprHeapRtv();

protected:
	std::shared_ptr<class Window> window;
	UINT bufferCount;
	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
	Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> swapChainBuffers;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeapRtv;
};