cbuffer ViewProjectionBuffer : register(b0)
{
    matrix viewProjectionMatrix;
}

cbuffer TransformBuffer : register(b1)
{
    matrix transform;
}

struct ObjectConstants
{
    uint isInstanced;
    uint useTransform;
    int padding[2];
};
cbuffer ObjectConstantsBuffer : register(b2)
{
    ObjectConstants constants;
}

StructuredBuffer<matrix> instances : register(t0);

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

PSInput Main(VSInput input, uint InstanceID : SV_InstanceID)
{
    PSInput result;
    float4 worldPos = input.position;
    if(constants.useTransform)
    {
        if(constants.isInstanced)
            worldPos = mul(instances[InstanceID], input.position);
        else
            worldPos = mul(transform, input.position);
    }
    float4 clipPos = mul(viewProjectionMatrix, worldPos);

    result.position = clipPos;
    result.fragPos = worldPos.xyz;
    result.normal = mul(float4(input.normal, 0.0f), transform).xyz;
    result.uv = input.uv;

    return result;
}
