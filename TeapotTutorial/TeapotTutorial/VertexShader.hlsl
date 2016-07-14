struct VertexData
{
	float3 pos : POSITION;
};

struct VertexToHull
{
	float3 pos : POSITION;
};

VertexToHull main(VertexData input)
{
	VertexToHull output;
	output.pos = input.pos;

	return output;
}