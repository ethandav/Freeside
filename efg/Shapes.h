#pragma once
#ifndef SHAPES_H
#define SHAPES_H

#include <vector>
#include <DirectXMath.h>

struct Vertex
{
    DirectX::XMFLOAT3 position  = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT3 normal    = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT2 uv        = DirectX::XMFLOAT2(0.0f, 0.0f);
};

struct Shape
{
    int                     vertexCount     = 0;
    int                     indexCount      = 0;
    std::vector<Vertex>  vertices        = {};
    std::vector<uint32_t>   indices         = {};
};

namespace Shapes
{
    enum Types
    {
        SQUARE,
        PLANE,
        GRID,
        CUBE,
        SKYBOX,
        SPHERE,
        TRIANGLE,
        PYRAMID
    };

    Shape getShape(Types type);
}

#endif