//#include "TestAppRS.hlsli"

struct VSOutput
{
	float4 pos : SV_Position;
	//float2 uv : TexCoord0;
};

//[RootSignature(TestApp_RootSig)]
float3 main(VSOutput input) : SV_TARGET0
{
	return float3(1.f, 1.f, 1.f);
	return float3(input.pos.x, input.pos.y, input.pos.z);
}
