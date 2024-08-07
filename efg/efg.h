#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <ResourceUploadBatch.h>
#include <WICTextureLoader.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <memory>
#include <vector>

#include "../DirectX-Headers/include/directx/d3dx12.h"
#include "DXHelper.h"
#include "efg_window.h"
#include "Shapes.h"

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

enum EfgResult
{
    EfgResult_NoError,
    EfgResult_InvalidContext,
    EfgResult_InvalidOperation,
    EfgResult_InvalidParameter,
    EfgResult_OutOfMemory,
    EfgResult_InternalError
};

enum EFG_RANGE_TYPE
{
    efgRange_CBV,
    efgRange_SRV,
    efgRange_SAMPLER
};

struct EfgResource
{
    uint32_t heapOffset = 0;
    uint32_t registerIndex = 0;
};

struct EfgBufferInternal : public EfgResource
{
    EFG_BUFFER_TYPE type = {};
    UINT size = 0;
    UINT alignmentSize = 0;
    ComPtr<ID3D12Resource> m_bufferResource;
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
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
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

struct EfgPSO
{
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
};

struct EfgProgram
{
    std::wstring source;
    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
};

struct EfgMaterial
{
    uint32_t index = 0;
    std::string diffuse_texname;
    EfgTexture diffuseTexture = {};
};

struct EfgInstanceBatch
{
    EfgVertexBuffer vertexBuffer = {};
    EfgIndexBuffer indexBuffer = {};
    uint32_t indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<EfgMaterial> materials;
};

class EfgDescriptorRange
{
public:
    EfgDescriptorRange(EFG_RANGE_TYPE type, uint32_t baseRegister, uint32_t descriptors = 0) : rangeType(type), baseShaderRegister(baseRegister), numDescriptors(descriptors) {};
    template<typename TYPE> void insert(TYPE& efgResource) {
        EfgResource* resource = reinterpret_cast<EfgResource*>(efgResource.handle);
        resource->registerIndex = baseShaderRegister + (uint32_t)resources.size();
        if (resources.size() == 0)
            offset = resource->heapOffset;
        if (resource->heapOffset < offset)
            offset = resource->heapOffset;
        resources.push_back(resource);
        numDescriptors++;
    };
    D3D12_DESCRIPTOR_RANGE Commit(bool useOffset);

    uint32_t numDescriptors = 0;
    UINT offset = 0;
private:
    EFG_RANGE_TYPE rangeType;
    uint32_t baseShaderRegister = 0;
    std::vector<EfgResource*> resources = {};
};

class EfgRootParameter
{
public:
    void insert(EfgDescriptorRange& range) {
        bool useOffset = !ranges.empty();
        if (useOffset && range.offset < data.offset) {
            data.offset = range.offset;
        } else if (!useOffset) {
            data.offset = range.offset;
        }
        ranges.push_back(range.Commit(useOffset));
        data.size += range.numDescriptors;
    };
    D3D12_ROOT_PARAMETER Commit();

    struct Data {
        uint32_t size = 0;
        UINT offset = 0;
        bool conditionalBind = false;
        std::vector<EfgDescriptorRange*> rangeData;
    };

    Data data;

private:
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
};

class EfgRootSignature
{
public:
    void insert(EfgRootParameter& parameter) { rootParameters.push_back(parameter.Commit()); parameterData.push_back(parameter.data); };
    ComPtr<ID3DBlob> Serialize();
    ComPtr<ID3D12RootSignature>& Get() { return rootSignature; }
    std::vector<EfgRootParameter::Data> parameterData = {};
    std::vector<D3D12_ROOT_PARAMETER> rootParameters = {};
private:
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    ComPtr<ID3D12RootSignature> rootSignature;

};

class EfgContext
{
public:
	void initialize(HWND window);
    EfgVertexBuffer CreateVertexBuffer(void const* data, UINT size);
    EfgIndexBuffer CreateIndexBuffer(void const* data, UINT size);
    void CreateDepthBuffer();
    EfgBuffer CreateConstantBuffer(void const* data, UINT size);
    EfgBuffer CreateStructuredBuffer(void const* data, UINT size, uint32_t numElements, size_t stride);
    EfgTexture CreateTexture2D(const wchar_t* filename);
    EfgSampler CreateSampler();
    void CreateRootSignature(EfgRootSignature& rootSignature);
    void UpdateConstantBuffer(EfgBuffer& buffer, void const* data, UINT size);
    void BindVertexBuffer(EfgVertexBuffer buffer);
    void BindIndexBuffer(EfgIndexBuffer buffer);
    void Bind2DTexture(const EfgTexture& texture);
    void BindRootDescriptorTable(EfgRootSignature& rootSignature);
    EfgResult CommitShaderResources();
    EfgProgram CreateProgram(LPCWSTR fileName);
    EfgPSO CreateGraphicsPipelineState(EfgProgram program, EfgRootSignature& rootSignature);
    void SetPipelineState(EfgPSO pso);
    void DrawInstanced(uint32_t vertexCount);
    void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount = 1);
    std::vector<EfgInstanceBatch> LoadFromObj(const char* basePath, const char* file);
    void Frame();
    void Render();
    void Destroy();
    void CheckD3DErrors();

    template<typename TYPE>
    EfgVertexBuffer CreateVertexBuffer(void const* data, uint32_t count)
    {
        EfgVertexBuffer buffer = CreateVertexBuffer(data, count * sizeof(TYPE));
        return buffer;
    }
    
    template<typename TYPE>
    EfgIndexBuffer CreateIndexBuffer(void const* data, uint32_t count)
    {
        EfgIndexBuffer buffer = CreateIndexBuffer(data, count * sizeof(TYPE));
        return buffer;
    }
    
    template<typename TYPE>
    EfgBuffer CreateConstantBuffer(void const* data, uint32_t count)
    {
        return CreateConstantBuffer(data, count * sizeof(TYPE));
    }
    
    template<typename TYPE>
    EfgBuffer CreateStructuredBuffer(void const* data, uint32_t count)
    {
        size_t stride = sizeof(TYPE);
        return CreateStructuredBuffer(data, count * sizeof(TYPE), count, stride);
    }

    std::vector<EfgMaterial> uploadMaterials;

private:
    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false
    );
    std::wstring GetAssetFullPath(LPCWSTR assetName);
    void LoadPipeline();
    void LoadAssets();
    void WaitForPreviousFrame();

    void CompileProgram(EfgProgram& program);
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);

    void ResetCommandList();
    void ExecuteCommandList();
    void WaitForGpu();

    void CreateBuffer(void const* data, EfgBufferInternal& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState);
    void CopyBuffer(ComPtr<ID3D12Resource> dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES current, D3D12_RESOURCE_STATES final);
    ComPtr<ID3D12Resource> CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size);
    void TransitionResourceState(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState);
    EfgResult CreateCbvSrvDescriptorHeap(uint32_t numDescriptors);
    void CreateSamplerDescriptorHeap(uint32_t samplerCount);
    EfgResult CreateConstantBufferView(EfgConstantBuffer* buffer, uint32_t heapOffset);
    EfgResult CreateStructuredBufferView(EfgStructuredBuffer* buffer, uint32_t heapOffset);
    void CreateTextureView(EfgTextureInternal* texture, uint32_t heapOffset);
    void CommitSampler(EfgSamplerInternal* sampler, uint32_t heapOffset);

	HWND window_ = {};
    static const UINT FrameCount = 2;
    bool useWarpDevice = false;
    uint32_t windowWidth = 0;
    uint32_t windowHeight = 0;
    float m_aspectRatio = 0.0f;
    std::wstring m_assetsPath;

    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12Resource> depthStencilBuffer;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    UINT m_rtvDescriptorSize = 0;
    UINT m_cbvSrvDescriptorSize = 0;
    UINT m_samplerDescriptorSize = 0;
    uint32_t m_cbvDescriptorCount = 0;
    uint32_t m_srvDescriptorCount = 0;
    uint32_t m_textureCount = 0;
    uint32_t m_samplerCount = 0;
    std::list<EfgConstantBuffer*> m_constantBuffers = {};
    std::list<EfgStructuredBuffer*> m_structuredBuffers = {};
    std::list<EfgTextureInternal*> m_textures = {};
    std::list<EfgSamplerInternal*> m_samplers = {};

    EfgPSO m_boundPSO = {};
    EfgVertexBuffer m_boundVertexBuffer = {};
    EfgIndexBuffer m_boundIndexBuffer = {};
    const EfgTexture* m_boundTexture = nullptr;


    // Synchronization objects.
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
};

XMMATRIX efgCreateTransformMatrix(XMFLOAT3 translation, XMFLOAT3 rotation, XMFLOAT3 scale);

