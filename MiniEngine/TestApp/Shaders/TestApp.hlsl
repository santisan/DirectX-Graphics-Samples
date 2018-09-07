
cbuffer VSConstants : register(b0)
{
	float4x4 worldViewProjection;
};

struct VSInput
{
	float3 position : POSITION;
	//float2 texcoord0 : TEXCOORD;
	//float3 normal : NORMAL;
	//float3 tangent : TANGENT;
	//float3 bitangent : BITANGENT;
};

struct VSOutput
{
	float4 pos : SV_Position;
	//float2 uv : TexCoord0;
};

VSOutput VSMain(VSInput vsInput)
{
	VSOutput vsOutput;
	vsOutput.pos = mul(worldViewProjection, float4(vsInput.position, 1.0));
	//vsOutput.uv = vsInput.texcoord0;
	return vsOutput;
}

float3 PSMain(VSOutput input) : SV_TARGET0
{
	//return float3(1.f, 1.f, 1.f);
	return float3(input.pos.x, input.pos.y, input.pos.z);
}
