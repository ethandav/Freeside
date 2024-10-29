#pragma once
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include "../DirectX-Headers/include/directx/d3dx12.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

typedef
enum EFG_BUFFER_TYPE 
{
    EFG_VERTEX_BUFFER,
    EFG_INDEX_BUFFER,
    EFG_CONSTANT_BUFFER,
    EFG_STRUCTURED_BUFFER
} EFG_BUFFER_TYPE;

typedef
enum EFG_CPU_ACCESS 
{
    EFG_CPU_NONE,
    EFG_CPU_READ,
    EFG_CPU_WRITE
} EFG_CPU_ACCESS;

class EfgResource
{
public:
    ID3D12Resource* Get() { return d3d12Resource.Get(); }
    ComPtr<ID3D12Resource> Ptr() { return d3d12Resource; }
    void Set(ComPtr<ID3D12Resource>resource) { d3d12Resource = resource; }

    uint32_t heapOffset = 0;
    uint32_t registerIndex = 0;
    D3D12_RESOURCE_STATES currState;
private:
    ComPtr<ID3D12Resource> d3d12Resource;
};

struct EfgBufferInternal : public EfgResource
{
    EFG_BUFFER_TYPE type = {};
    UINT size = 0;
    UINT alignmentSize = 0;
};

struct EfgVertexBuffer : public EfgBufferInternal
{
    D3D12_VERTEX_BUFFER_VIEW view = {};
};

struct EfgIndexBuffer : public EfgBufferInternal
{
    D3D12_INDEX_BUFFER_VIEW view = {};
    uint32_t indexCount = 0;
};

struct EfgConstantBuffer : public EfgBufferInternal
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle = {};
};

struct EfgStructuredBuffer : public EfgBufferInternal
{
    uint32_t count = 0;
    size_t stride = 0;
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
};

struct EfgBuffer
{
    uint32_t index = 0;
    uint64_t handle = 0;
};

struct EfgTextureInternal : EfgResource
{
    ComPtr<ID3D12Resource> resource;
    DXGI_FORMAT format;
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
};

struct EfgTexture
{
    uint32_t index = 0;
    uint64_t handle = 0;
};

struct EfgSamplerInternal : EfgResource
{
    D3D12_SAMPLER_DESC desc = {};
};

struct EfgSampler
{
    uint64_t handle = 0;
};

struct EfgMaterialTextures
{
    EfgTexture diffuse_map;
    EfgTexture roughness_map;
    EfgTexture metallicity_map;
    EfgTexture emissivity_map;
    EfgTexture specular_map;
    EfgTexture normal_map;
    EfgTexture transmission_map;
    EfgTexture sheen_map;
    EfgTexture clearcoat_map;
    EfgTexture clearcoat_roughness_map;
    EfgTexture ao_map;
};

struct EfgMaterialBuffer
{
    XMFLOAT4 ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4 diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4 specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4 transmittance = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    XMFLOAT4 emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    float shininess = 0.0f;
    float roughness = 1.0f;
    float metallic = 0.0f;
    float ior = 1.5f;
    float dissolve = 1.5f;
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    int diffuseMapFlag = 0;
    float padding[3] = { 0.0f, 0.0f, 0.0f };
};

