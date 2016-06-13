struct VertexInput
{
	float3 pos : POSITION;
	float3 color : COLOR;
};

struct VertexOutput
{
	float3 color : COLOR;
	float4 pos : SV_POSITION;
};

VertexOutput main(VertexInput input)
{
	VertexOutput output;
	output.color = input.color;
	output.pos = float4(input.pos, 1.0f);

	return output;
}

/*float4 main() : SV_POSITION
{
	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}*/