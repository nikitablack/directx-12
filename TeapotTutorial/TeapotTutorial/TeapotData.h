#pragma once

#include <vector>
#include <DirectXMath.h>

struct TeapotData
{
	static std::vector<DirectX::XMFLOAT3> points;
	static std::vector<uint32_t> patches;
	static std::vector<DirectX::XMFLOAT4X4> patchesTransforms;
	static std::vector<DirectX::XMFLOAT3> patchesColors;

private:
	static DirectX::XMFLOAT4X4 getRotationMatrix(float x, float y, float z)
	{
		DirectX::XMFLOAT4X4 tmp;
		DirectX::XMStoreFloat4x4(&tmp, DirectX::XMMatrixRotationRollPitchYaw(x, z, y));
		return tmp;
	}

	static DirectX::XMFLOAT4X4 getScalingMatrix(float x, float y, float z)
	{
		DirectX::XMFLOAT4X4 tmp;
		DirectX::XMStoreFloat4x4(&tmp, DirectX::XMMatrixScaling(x, y, z));
		return tmp;
	}
};