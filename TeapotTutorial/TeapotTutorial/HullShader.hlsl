#define NUM_CONTROL_POINTS 16

struct PatchTesselationFactors
{
	int edge;
	int inside;
};
ConstantBuffer<PatchTesselationFactors> tessFactors : register(b0);

struct VertexToHull
{
	float3 pos : POSITION;
};

struct PatchConstantData
{
	float EdgeTessFactor[4] : SV_TessFactor;
	float InsideTessFactor[2] : SV_InsideTessFactor;
};

struct HullToDomain
{
	float3 pos : POSITION;
};

PatchConstantData CalculatePatchConstants(InputPatch<VertexToHull, NUM_CONTROL_POINTS> ip, uint patchID : SV_PrimitiveID)
{
	PatchConstantData Output;

	Output.EdgeTessFactor[0] = tessFactors.edge;
	Output.EdgeTessFactor[1] = tessFactors.edge;
	Output.EdgeTessFactor[2] = tessFactors.edge;
	Output.EdgeTessFactor[3] = tessFactors.edge;
	Output.InsideTessFactor[0] = tessFactors.inside;
	Output.InsideTessFactor[1] = tessFactors.inside;

	return Output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalculatePatchConstants")]
HullToDomain main(InputPatch<VertexToHull, NUM_CONTROL_POINTS> ip, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
	HullToDomain output;
	output.pos = ip[i].pos;

	return output;
}