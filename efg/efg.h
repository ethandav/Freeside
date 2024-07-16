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

struct EfgBuffer : public EfgResource
{
    EFG_BUFFER_TYPE type = {};
    UINT size = 0;
    UINT alignmentSize = 0;
    ComPtr<ID3D12Resource> m_bufferResource;
};

struct EfgVertexBuffer : public EfgBuffer
{
    D3D12_VERTEX_BUFFER_VIEW view = {};
};

struct EfgIndexBuffer : public EfgBuffer
{
    D3D12_INDEX_BUFFER_VIEW view = {};
};

struct EfgConstantBuffer : public EfgBuffer
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle = {};
};

struct EfgStructuredBuffer: public EfgBuffer
{
    uint32_t count = 0;
    size_t stride = 0;
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
};

struct EfgTexture : EfgResource
{
    uint32_t index = 0;
    ComPtr<ID3D12Resource> resource;
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = {};
};

struct EfgSampler : EfgResource
{
    D3D12_SAMPLER_DESC desc = {};
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

class EfgDescriptorRange
{
public:
    EfgDescriptorRange(EFG_RANGE_TYPE type, uint32_t baseRegister) : rangeType(type), baseShaderRegister(baseRegister) {};
    template<typename TYPE> void insert(TYPE& resource) { resource.registerIndex = baseShaderRegister + (uint32_t)resources.size(); resources.push_back(&resource); resourceCount++; };
    D3D12_DESCRIPTOR_RANGE Commit();

    uint32_t resourceCount = 0;
private:
    EFG_RANGE_TYPE rangeType;
    uint32_t baseShaderRegister = 0;
    std::vector<EfgResource*> resources = {};
};

class EfgRootParameter
{
public:
    void insert(EfgDescriptorRange& range) { ranges.push_back(range.Commit()); size += range.resourceCount; };
    D3D12_ROOT_PARAMETER Commit();
    uint32_t size = 0;
private:
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
};

class EfgRootSignature
{
public:
    void insert(EfgRootParameter& parameter) { rootParameters.push_back(parameter.Commit()); parameterSizes.push_back(parameter.size); };
    ComPtr<ID3DBlob> Serialize();
    ComPtr<ID3D12RootSignature>& Get() { return rootSignature; }
    std::vector<uint32_t> parameterSizes = {};
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
    void CreateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size);
    void CreateStructuredBuffer(EfgStructuredBuffer& buffer, void const* data, UINT size, uint32_t numElements, size_t stride);
    void CreateTexture2D(EfgTexture& texture, const wchar_t* filename);
    void CreateSampler(EfgSampler & sampler);
    void CreateRootSignature(EfgRootSignature& rootSignature);
    void UpdateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size);
    void BindVertexBuffer(EfgVertexBuffer buffer);
    void BindIndexBuffer(EfgIndexBuffer buffer);
    void Bind2DTexture(const EfgTexture& texture);
    void BindRootDescriptorTable(EfgRootSignature& rootSignature);
    EfgResult CommitShaderResources();
    EfgProgram CreateProgram(LPCWSTR fileName);
    EfgPSO CreateGraphicsPipelineState(EfgProgram program, EfgRootSignature& rootSignature);
    void SetPipelineState(EfgPSO pso);
    void DrawInstanced(uint32_t vertexCount);
    void DrawIndexedInstanced(uint32_t indexCount);
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
    void CreateConstantBuffer(EfgConstantBuffer& buffer, void const* data, uint32_t count)
    {
        CreateConstantBuffer(buffer, data, count * sizeof(TYPE));
    }
    
    template<typename TYPE>
    void CreateStructuredBuffer(EfgStructuredBuffer& buffer, void const* data, uint32_t count)
    {
        size_t stride = sizeof(TYPE);
        CreateStructuredBuffer(buffer, data, count * sizeof(TYPE), count, stride);
    }

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

    void CreateBuffer(void const* data, EfgBuffer& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState);
    void CopyBuffer(ComPtr<ID3D12Resource> dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES current, D3D12_RESOURCE_STATES final);
    ComPtr<ID3D12Resource> CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size);
    void TransitionResourceState(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState);
    EfgResult CreateCbvSrvDescriptorHeap(uint32_t numDescriptors);
    void CreateSamplerDescriptorHeap(uint32_t samplerCount);
    EfgResult CreateConstantBufferView(EfgConstantBuffer* buffer, uint32_t heapOffset);
    EfgResult CreateStructuredBufferView(EfgStructuredBuffer* buffer, uint32_t heapOffset);
    void CreateTextureView(EfgTexture* texture, uint32_t heapOffset);
    void CommitSampler(EfgSampler * sampler, uint32_t heapOffset);

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
    std::list<EfgTexture*> m_textures = {};
    std::list<EfgSampler*> m_samplers = {};

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

