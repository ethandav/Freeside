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
TextureCube shadowCubeMap : register(t4);
SamplerState textureSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
SamplerComparisonState shadowCubeSampler : register(s2);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragPos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

static const float3 gridSamplingDisk[20] = {
    float3(1, 1,  1), float3( 1, -1,  1), float3(-1, -1,  1), float3(-1, 1,  1), 
    float3(1, 1, -1), float3( 1, -1, -1), float3(-1, -1, -1), float3(-1, 1, -1),
    float3(1, 1,  0), float3( 1, -1,  0), float3(-1, -1,  0), float3(-1, 1,  0),
    float3(1, 0,  1), float3(-1,  0,  1), float3( 1,  0, -1), float3(-1, 0, -1),
    float3(0, 1,  1), float3( 0, -1,  1), float3( 0, -1, -1), float3( 0, 1, -1)
};

float CalculateDirShadow(float3 fragPos, float3 normal, float3 lightDir) {
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

    float shadow = CalculateDirShadow(fragPos, normal, lightDir);

    if(mat.diffuseMapFlag > 0)
        texDiffuse = diffuseMap.Sample(textureSampler, uv).xyz;
    else
        texDiffuse = mat.diffuse.xyz;

    ambient = (dirLight.ambient * dirLight.color).xyz * texDiffuse;
    diffuse = (dirLight.diffuse.xyz * dirLight.color.xyz) * diff * texDiffuse * shadow;
    specular = dirLight.specular.xyz * spec * mat.specular.xyz * shadow;

    return ambient + diffuse + specular;
}

float CalcPointLightShadow(float3 fragPos, float3 lightPos)
{
    float3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    float shadow = 0.0f;
    float bias = 0.15;
    int samples = 20;
    float farPlane = 10.0f;
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / farPlane)) / 25.0;
    for (int i = 0; i < samples; ++i)
    {
        float closestDepth = shadowCubeMap.Sample(textureSampler, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= farPlane;
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return shadow;

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

    float shadow = CalcPointLightShadow(fragPos, light.position.xyz);

    if(mat.diffuseMapFlag > 0)
    {
        texDiffuse = diffuseMap.Sample(textureSampler, uv).xyz;
    }
    else
    {
        texDiffuse = mat.diffuse.xyz;
    }

    ambient = (light.ambient * light.color).xyz * texDiffuse;
    diffuse = (light.diffuse.xyz * light.color.xyz) * diff * texDiffuse * (1.0 - shadow);
    specular = light.specular.xyz * spec * mat.specular.xyz * (1.0 - shadow);

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return ambient + diffuse + specular;
}

float3 VisualizeSampledDirection(float3 fragPos, float3 lightPos)
{
    float3 lightToFrag = fragPos - lightPos;
    float3 direction = normalize(lightToFrag);

    // Map direction to color for visualization
    // This will show which axis (face) is dominant based on color
    return 0.5 * (direction + 1.0); // Shift and scale to [0, 1] range for visualization
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
    
    //return float4(visualizeShadowMapDepth(input.fragPos, lights[0].position.xyz), 1.0f);
    return float4(color, 1.0f);
}