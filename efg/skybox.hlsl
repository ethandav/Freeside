cbuffer ViewBuffer : register(b0)
{
    matrix viewMatrix;
}

cbuffer ProjBuffer : register(b1)
{
    matrix projMatrix;
}

TextureCube cubeMap : register(t0);
SamplerState textureSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 uv : TEXCOORD;
};

PSInput VSMain(float4 position : POSITION)
{
    PSInput result;
    float4 worldPos = position;
    matrix viewProj = mul(projMatrix, viewMatrix);
    float4 clipPos = mul(viewProj, worldPos);

    result.position = clipPos;
    result.uv = position;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = cubeMap.Sample(textureSampler, input.uv);
    return color;
}
