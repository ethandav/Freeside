#pragma once
#include <DirectXMath.h>

#include "efg.h"

using namespace DirectX;

struct Camera
{
    XMFLOAT3 eye;
    XMFLOAT3 center;
    XMFLOAT3 up;

    XMFLOAT4X4 view;
    XMFLOAT4X4 proj;
    XMFLOAT4X4 viewProj;

    XMFLOAT4X4 preView;
    XMFLOAT4X4 prevProj;
    XMFLOAT4X4 prevViewProj;
    float prevYaw, prevPitch;
};

Camera efgCreateCamera(EfgContext efg, const XMFLOAT3& eye, const XMFLOAT3& center);
void efgUpdateCamera(EfgContext efg, EfgWindow window, Camera& camera);
