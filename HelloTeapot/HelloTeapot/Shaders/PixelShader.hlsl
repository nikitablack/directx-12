struct VertexOutput
{
	float3 color : COLOR;
	float4 pos : SV_POSITION;
};

float4 main(VertexOutput input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}