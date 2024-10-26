#pragma once

struct PointLightBuffer
{
    XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 position = XMFLOAT4(0.0f, 0.0f, -2.0f, 0.0f);
    XMFLOAT4 ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 attenuation = XMFLOAT4(1.0f, 0.009f, 0.0032f, 0.0f);
};

struct DirLightBuffer
{
    XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 direction = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    XMFLOAT4 specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
};

struct LightConstants
{
    uint32_t numPointLights = 0;
};
