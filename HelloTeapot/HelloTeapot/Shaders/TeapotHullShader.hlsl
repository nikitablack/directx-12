#include "TeapotCommon.hlslinc"

// Patch Constant Function
PatchConstantData CalcHSPatchConstants(InputPatch<VertexData, NUM_CONTROL_POINTS> ip, uint PatchID : SV_PrimitiveID)
{
	PatchConstantData Output;

	Output.EdgeTessFactor[0] = EDGE_TESS_FACTOR;
	Output.EdgeTessFactor[1] = EDGE_TESS_FACTOR;
	Output.EdgeTessFactor[2] = EDGE_TESS_FACTOR;
	Output.EdgeTessFactor[3] = EDGE_TESS_FACTOR;
	Output.InsideTessFactor[0] = INSIDE_TESS_FACTOR;
	Output.InsideTessFactor[1] = INSIDE_TESS_FACTOR;

	return Output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalcHSPatchConstants")]
VertexData main(InputPatch<VertexData, NUM_CONTROL_POINTS> ip, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
	VertexData output;
	output.pos = ip[i].pos;

	return output;
}