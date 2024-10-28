cbuffer ViewBuffer : register(b1)
{
    float3 viewPos;
}

cbuffer LightConstants : register(b2)
{
    uint numPointlights;
}

cbuffer DirLightViewProj : register(b3)
{
    matrix dirLightViewProj;
}

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

cbuffer MatBuffer : register(b6)
{
    EfgMaterialBuffer mat;
}

struct DirLightBuffer
{
    float4 color;
    float4 direction;
    float4 ambient;
    float4 diffuse;
    float4 specular;
};
cbuffer DirLight : register(b7)
{
    DirLightBuffer dirLight;
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

Texture2D diffuseMap: register(t2);
Texture2D shadowMap: register(t3);
SamplerState textureSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float CalculateShadow(float3 fragPos, float3 normal, float3 lightDir) {
    // Transform world position to light space
    float4 lightSpacePos = mul(dirLightViewProj, float4(fragPos, 1.0));
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w; // Perspective divide
    projCoords = projCoords * 0.5f + 0.5f;                   // Map to [0, 1] UV coordinates

    // Early return if outside shadow map bounds
    if (projCoords.x < 0.0f || projCoords.x > 1.0f || projCoords.y < 0.0f || projCoords.y > 1.0f) {
        return 1.0f; // No shadow if outside bounds
    }

    // Calculate dynamic bias based on normal and light direction
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float shadow = 0.0f;

    // Perform PCF with dynamic bias
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float2 offset = float2(x, y) * 0.001f;
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + offset, lightSpacePos.z - bias);
        }
    }

    // Average the shadow samples for smoothness
    shadow /= 9.0f;
    return shadow;
}

float3 calculateDirLight(float3 fragPos, float3 normal, float3 viewDir, float2 uv)
{
    float3 lightDir = normalize(-dirLight.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), mat.shininess);

    float3 texDiffuse, ambient, diffuse, specular;

    float shadow = CalculateShadow(fragPos, normal, lightDir);

    if(mat.diffuseMapFlag > 0)
        texDiffuse = diffuseMap.Sample(textureSampler, uv).xyz;
    else
        texDiffuse = mat.diffuse.xyz;

    ambient = (dirLight.ambient * dirLight.color).xyz * texDiffuse;
    diffuse = (dirLight.diffuse.xyz * dirLight.color.xyz) * diff * texDiffuse * shadow;
    specular = dirLight.specular.xyz * spec * mat.specular.xyz * shadow;

    return ambient + diffuse + specular;

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

float4 Main(PSInput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(viewPos - input.fragPos);
    
    float3 color = float3(0.0f, 0.0f, 0.0);

    color += calculateDirLight(input.fragPos, normal, viewDir, input.uv);

    for (uint i = 0; i < numPointlights; ++i)
    {
        color += calculatePointLight(lights[i], normal, input.fragPos, viewDir, input.uv);
    }
    return float4(color, 1.0f);
}