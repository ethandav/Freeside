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

cbuffer ViewBuffer : register(b1)
{
    float3 viewPos;
}

cbuffer LightConstants : register(b2)
{
    uint lightCount;
}

cbuffer TransformBuffer : register(b3)
{
    matrix transform;
}

cbuffer ObjectConstants : register(b4)
{
    bool isInstanced;
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

StructuredBuffer<matrix> instances : register(t1);

Texture2D diffuseMap: register(t2);
SamplerState textureSampler : register(s0);

struct EfgMaterialBuffer
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 transmittance;
    float4 emission;
    float shininess;
    float roughness;
    float metallic;
    float ior;
    float dissolve;
    float clearcoat;
    float clearcoat_roughness;
    int diffuseMapFlag;
    float padding[3];
};

cbuffer MatBuffer : register(b5)
{
    EfgMaterialBuffer mat;
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

PSInput VSMain(VSInput input, uint InstanceID : SV_InstanceID)
{
    PSInput result;
    float4 worldPos;
    if(isInstanced)
        worldPos = mul(instances[InstanceID], input.position);
    else
        worldPos = mul(transform, input.position);
    float4 clipPos = mul(viewProjectionMatrix, worldPos);

    result.position = clipPos;
    result.fragPos = worldPos.xyz;
    result.normal = mul(float4(input.normal, 0.0f), transform).xyz;
    result.uv = input.uv;

    return result;
}

float3 calculatePointLight(LightData light, float3 normal, float3 fragPos, float3 viewDir, float2 uv)
{ 
    float3 lightDir = normalize(light.position.xyz - fragPos);

    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), mat.shininess);

    // Attenuation
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * (distance * distance));

    float3 texDiffuse, ambient, diffuse, specular;

    if(mat.diffuseMapFlag > 0)
    {
        texDiffuse = diffuseMap.Sample(textureSampler, uv).xyz;
    }
    else
    {
        texDiffuse = mat.diffuse.xyz;
    }

    ambient = (light.ambient * light.color).xyz * texDiffuse;
    diffuse = (light.diffuse.xyz * light.color.xyz) * diff * texDiffuse;
    specular = light.specular.xyz * spec * mat.specular.xyz;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return ambient + diffuse + specular;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(viewPos - input.fragPos);
    
    float3 color;

    for (uint i = 0; i < lightCount; ++i)
    {
        color += calculatePointLight(lights[i], normal, input.fragPos, viewDir, input.uv);
    }
    return float4(color, 1.0f);
}