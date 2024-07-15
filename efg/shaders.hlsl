//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

cbuffer ViewProjectionBuffer : register(b0)
{
    matrix viewProjectionMatrix;
}

cbuffer TransformBuffer : register(b1)
{
    matrix transform;
}


cbuffer ViewBuffer : register(b2)
{
    float3 viewPos;
}

struct Material
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float shininess;
    float3 padding;
};

cbuffer MaterialBuffer : register(b3)
{
    Material material;
}

struct LightData
{
    float4 color;
    float4 position;
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 attenuation;
};
StructuredBuffer<LightData> lights : register(t0);

Texture2D diffuseMap: register(t1);
SamplerState textureSampler : register(s0);

cbuffer LightConstants : register(b4)
{
    uint lightCount;
}

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

PSInput VSMain(VSInput input)
{
    PSInput result;
    float4 worldPos = mul(transform, input.position);
    float4 clipPos = mul(viewProjectionMatrix, worldPos);

    result.position = clipPos;
    result.fragPos = worldPos.xyz;
    result.normal = mul(float4(input.normal, 0.0f), transform).xyz;
    result.uv = input.uv;

    return result;
}

float3 calculatePointLight(LightData light, float3 normal, float3 fragPos, float3 viewDir)
{
    // Diffuse
    float3 lightDir = normalize(light.position.xyz - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // Attenuation
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * (distance * distance));

    // Material
    float3 ambient = (light.ambient * light.color) * material.ambient;
    float3 diffuse = (light.diffuse.xyz * light.color.xyz) * (diff * material.diffuse.xyz);
    float3 specular = light.specular.xyz * (spec * material.specular.xyz);

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(viewPos - input.fragPos);
    
    float3 color = diffuseMap.Sample(textureSampler, input.uv);

    for (uint i = 0; i < lightCount; ++i)
    {
        color += calculatePointLight(lights[i], normal, input.fragPos, viewDir);
    }
    return float4(color, 1.0f);
}