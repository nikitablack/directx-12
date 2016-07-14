// http://www.gdcvault.com/play/1012740/direct3d

#define NUM_CONTROL_POINTS 16

struct ConstantBufferPerObj
{
	row_major float4x4 wvpMat;
};
ConstantBuffer<ConstantBufferPerObj> constPerObject : register(b0);

struct PatchTransform
{
	row_major float4x4 transform;
};
StructuredBuffer<PatchTransform> patchTransforms : register(t0);

struct PatchColor
{
	float3 color;
};
StructuredBuffer<PatchColor> patchColors : register(t1);

struct PatchConstantData
{
	float EdgeTessFactor[4] : SV_TessFactor;
	float InsideTessFactor[2] : SV_InsideTessFactor;
};

struct HullToDomain
{
	float3 pos : POSITION;
};

struct DomainToPixel
{
	float4 pos : SV_POSITION;
	float3 color : COLOR;
};

float4 BernsteinBasis(float t)
{
	float invT = 1.0f - t;
	return float4(invT * invT * invT,	// (1-t)3
		3.0f * t * invT * invT,			// 3t(1-t)2
		3.0f * t * t * invT,			// 3t2(1-t)
		t * t * t);						// t3
}

float4 dBernsteinBasis(float t)
{
	float invT = 1.0f - t;
	return float4(-3 * invT * invT,		// -3(1-t)2
		3 * invT * invT - 6 * t * invT,	// 3(1-t)-6t(1-t)
		6 * t * invT - 3 * t * t,		// 6t(1-t) – 3t2
		3 * t * t);						// 3t2
}

float3 EvaluateBezier(const OutputPatch<HullToDomain, NUM_CONTROL_POINTS> bezpatch, float4 BasisU, float4 BasisV)
{
	float3 Value = float3(0, 0, 0);
	Value = BasisV.x * (bezpatch[0].pos * BasisU.x + bezpatch[1].pos * BasisU.y + bezpatch[2].pos * BasisU.z + bezpatch[3].pos * BasisU.w);
	Value += BasisV.y * (bezpatch[4].pos * BasisU.x + bezpatch[5].pos * BasisU.y + bezpatch[6].pos * BasisU.z + bezpatch[7].pos * BasisU.w);
	Value += BasisV.z * (bezpatch[8].pos * BasisU.x + bezpatch[9].pos * BasisU.y + bezpatch[10].pos * BasisU.z + bezpatch[11].pos * BasisU.w);
	Value += BasisV.w * (bezpatch[12].pos * BasisU.x + bezpatch[13].pos * BasisU.y + bezpatch[14].pos * BasisU.z + bezpatch[15].pos * BasisU.w);

	return Value;
}

[domain("quad")]
DomainToPixel main(PatchConstantData input, float2 domain : SV_DomainLocation, const OutputPatch<HullToDomain, NUM_CONTROL_POINTS> patch, uint PatchID : SV_PrimitiveID)
{
	// Evaluate the basis functions at (u, v)
	float4 BasisU = BernsteinBasis(domain.x);
	float4 BasisV = BernsteinBasis(domain.y);
	float4 dBasisU = dBernsteinBasis(domain.x);
	float4 dBasisV = dBernsteinBasis(domain.y);

	// Evaluate the surface position for this vertex
	float3 WorldPos = EvaluateBezier(patch, BasisU, BasisV);

	// Evaluate the tangent space for this vertex (using derivatives)
	float3 Tangent = EvaluateBezier(patch, dBasisU, BasisV);
	float3 BiTangent = EvaluateBezier(patch, BasisU, dBasisV);
	float3 Norm = normalize(cross(Tangent, BiTangent));

	float4x4 transform = patchTransforms[PatchID].transform;
	float4 WorldPosTransformed = mul(float4(WorldPos, 1.0f), transform);
	DomainToPixel output;
	output.pos = mul(WorldPosTransformed, constPerObject.wvpMat);
	output.color = patchColors[PatchID].color;

	return output;
}