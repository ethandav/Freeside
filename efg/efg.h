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
#include "efg_resources.h"
#include "efg_lighting.h"
#include "efg_gameObject.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

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

enum EFG_ROOT_PARAMETER_TYPE
{
    efgRootParamter_DESCRIPTOR_TABLE,
    efgRootParameter_CONSTANT,
    efgRootParameter_CBV,
    efgRootParamter_SRV,
    efgRootParamter_UAV
};

struct EfgPSOInternal
{
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
};

struct EfgPSO
{
    uint64_t handle = 0;
};

struct EfgShader
{
    std::wstring source;
    ComPtr<ID3DBlob> byteCode;
};

struct EfgProgram
{
    EfgShader vertexShader;
    EfgShader pixelShader;
};

struct EfgInstanceBatch
{
    EfgBuffer vertexBuffer = {};
    EfgBuffer indexBuffer = {};
    uint32_t indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct EfgImportMesh
{
    std::vector<EfgBuffer> materialBuffers;
    std::vector<EfgMaterialTextures> textures;
    std::unordered_map<size_t, EfgInstanceBatch> materialBatches;
    ObjectConstants constants;
    EfgBuffer constantsBuffer;
};

struct ShaderRegisters
{
    uint32_t CBV = 0;
    uint32_t SRV = 0;
    uint32_t UAV = 0;
    uint32_t SAMPLER = 0;
};

class EfgDescriptorRange
{
public:
    EfgDescriptorRange(EFG_RANGE_TYPE type, uint32_t baseRegister, uint32_t descriptors = 0) : rangeType(type), baseShaderRegister(baseRegister), numDescriptors(descriptors) {};
    template<typename TYPE> void insert(TYPE& efgResource) {
        EfgResource* resource = reinterpret_cast<EfgResource*>(efgResource.handle);
        if (numDescriptors == 0)
            heapOffset = resource->heapOffset;
        if (resource->heapOffset < heapOffset)
            heapOffset = resource->heapOffset;
        numDescriptors++;
    };
    D3D12_DESCRIPTOR_RANGE Commit(uint32_t rangeOffset);
    EFG_RANGE_TYPE GetType() { return rangeType; }

    uint32_t numDescriptors = 0;
    UINT heapOffset = 0;
    UINT rangeOffset = 0;
private:
    EFG_RANGE_TYPE rangeType;
    uint32_t baseShaderRegister = 0;
};

class EfgRootParameter
{
public:
    EfgRootParameter(EFG_ROOT_PARAMETER_TYPE efgType)
    {
        switch (efgType)
        {
        case 0:
            type = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            break;
        case 1:
            type = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            break;
        case 2:
            type = D3D12_ROOT_PARAMETER_TYPE_CBV;
            break;
        case 3:
            type = D3D12_ROOT_PARAMETER_TYPE_SRV;
            break;
        case 4:
            type = D3D12_ROOT_PARAMETER_TYPE_UAV;
            break;
        default:
            throw("Root paramter must have a type");
        }
    };
    void insert(EfgDescriptorRange& range) {
        bool useOffset = !ranges.empty();
        if (useOffset && range.heapOffset < data.offset) {
            data.offset = range.heapOffset;
        } else if (!useOffset) {
            data.offset = range.heapOffset;
        }
        if (ranges.empty())
        {
            switch (range.GetType())
            {
            case efgRange_CBV:
            case efgRange_SRV:
                data.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                break;
            case efgRange_SAMPLER:
                data.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                break;
            }
        }
        else
        {
            if (range.GetType() != data.heapType)
                throw("Cannot mix heap types!");
        }
        ranges.push_back(range.Commit((UINT)ranges.size()));
        data.descriptorSize += range.numDescriptors;
    };
    D3D12_ROOT_PARAMETER Commit(ShaderRegisters& registerIndex);

    struct Data {
        UINT offset = 0;
        UINT index = 0;
        UINT descriptorSize = 0;
        D3D12_DESCRIPTOR_HEAP_TYPE heapType = {};
    };

    D3D12_ROOT_PARAMETER_TYPE type;
    Data data;

private:
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
};

class EfgRootSignature
{
public:
    void insert(EfgRootParameter& parameter) {
        parameter.data.index = (rootParameters.empty()) ? 0 : (UINT)rootParameters.size();
        rootParameters.push_back(parameter.Commit(registers));
        if(parameter.type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            descriptorTables.push_back(parameter);
    };
    ComPtr<ID3DBlob> Serialize();
    void Destroy() { rootSignature.Reset(); };
    ComPtr<ID3D12RootSignature>& Get() { return rootSignature; }
    std::vector<EfgRootParameter> descriptorTables = {};
    std::vector<D3D12_ROOT_PARAMETER> rootParameters = {};
private:
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    ComPtr<ID3D12RootSignature> rootSignature;
    ShaderRegisters registers = {};
};

class EfgContext
{
public:
	void initialize(HWND window);
    EfgBuffer CreateVertexBuffer(void const* data, UINT size);
    EfgBuffer CreateIndexBuffer(void const* data, UINT size);
    EfgBuffer CreateConstantBuffer(void const* data, UINT size);
    EfgBuffer CreateStructuredBuffer(void const* data, UINT size, uint32_t numElements, size_t stride);
    EfgTexture CreateDepthBuffer(uint32_t width, uint32_t height);
    EfgTexture CreateShadowMap(uint32_t width, uint32_t height);
    EfgTexture CreateTexture2D();
    EfgTexture CreateTexture2DFromFile(const wchar_t* filename);
    EfgTexture CreateTextureCube(const std::vector<std::wstring>& filenames);
    EfgTexture CreateCubeShadowMap(uint32_t width, uint32_t height);
    EfgSampler CreateTextureSampler();
    EfgSampler CreateDepthCubeSampler();
    EfgSampler CreateDepthSampler();
    EfgTexture CreateColorBuffer(uint32_t width, uint32_t height);
    void Copy2DTextureToBackbuffer(EfgTexture);
    void ClearRenderTargetView(EfgTexture texture);
    void ClearDepthStencilView(EfgTexture texture);
    void CreateRootSignature(EfgRootSignature& rootSignature);
    void UpdateConstantBuffer(EfgBuffer& buffer, void const* data, UINT size);
    void UpdateStructuredBuffer(EfgBuffer& buffer, void const* data, UINT size);
    void BindVertexBuffer(EfgBuffer buffer);
    void BindIndexBuffer(EfgBuffer buffer);
    void Bind2DTexture(uint32_t index, const EfgTexture& texture);
    void BindConstantBuffer(uint32_t index, const EfgBuffer& buffer);
    void BindStructuredBuffer(uint32_t index, const EfgBuffer& buffer);
    void BindRootDescriptorTable(EfgRootSignature& rootSignature);
    EfgResult CommitShaderResources();
    EfgShader CreateShader(LPCWSTR fileName, LPCSTR target, LPCSTR entryPoint = "Main");
    EfgPSO CreateGraphicsPipelineState(EfgProgram program, EfgRootSignature& rootSignature);
    EfgPSO CreateShadowMapPSO(EfgProgram program, EfgRootSignature rootSignature);
    void SetPipelineState(EfgPSO pso);
    void SetRenderTarget(EfgTexture texture, uint32_t offset = 0, EfgTexture* depthStencil = nullptr);
    void SetRenderTargetResolution(uint32_t width, uint32_t height);
    void DrawInstanced(uint32_t vertexCount);
    void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount = 1);
    EfgImportMesh LoadFromObj(const char* basePath, const char* file);
    void Frame();
    void Render();
    void OpenCommandList();
    void ExecuteCommandList();
    void WaitForGpu();
    void Destroy();
    void CheckD3DErrors();

    template<typename TYPE>
    EfgBuffer CreateVertexBuffer(void const* data, uint32_t count)
    {
        EfgBuffer buffer = CreateVertexBuffer(data, count * sizeof(TYPE));
        return buffer;
    }
    
    template<typename TYPE>
    EfgBuffer CreateIndexBuffer(void const* data, uint32_t count)
    {
        EfgBuffer buffer = CreateIndexBuffer(data, count * sizeof(TYPE));
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

    void CompileShader(EfgShader& shader, LPCSTR entryPoint, LPCSTR target);
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);


    void CreateBuffer(void const* data, EfgBufferInternal& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState);
    void CopyBuffer(EfgResource* dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES current, D3D12_RESOURCE_STATES final);
    ComPtr<ID3D12Resource> CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size);
    void TransitionResourceState(EfgResource* resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState);
    EfgResult CreateCbvSrvDescriptorHeap(uint32_t numDescriptors);
    void CreateSamplerDescriptorHeap(uint32_t samplerCount);
    EfgResult CreateConstantBufferView(EfgConstantBuffer* buffer, uint32_t heapOffset);
    EfgResult CreateStructuredBufferView(EfgStructuredBuffer* buffer, uint32_t heapOffset);
    void CreateTextureView(EfgTextureInternal* texture, uint32_t heapOffset);
    void CreateTextureCubeView(EfgTextureInternal* texture, uint32_t heapOffset);
    void CommitSampler(EfgSamplerInternal* sampler, uint32_t heapOffset);
    void ClearDepthStencilViewCube(EfgTextureInternal* texture);

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
    ComPtr<ID3D12Resource> m_backBuffers[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_backBufferHeap;
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
    UINT m_dsvDescriptorSize = 0;
    uint32_t m_cbvDescriptorCount = 0;
    uint32_t m_srvDescriptorCount = 0;
    uint32_t m_dsvDescriptorCount = 0;
    uint32_t m_rtvDescriptorCount = 0;
    uint32_t m_textureCount = 0;
    uint32_t m_textureCubeCount = 0;
    uint32_t m_samplerCount = 0;
    std::list<EfgConstantBuffer*> m_constantBuffers = {};
    std::list<EfgStructuredBuffer*> m_structuredBuffers = {};
    std::list<EfgTextureInternal*> m_textures = {};
    std::list<EfgTextureInternal*> m_textureCubes = {};
    std::list<EfgSamplerInternal*> m_samplers = {};
    std::list<EfgRootSignature*> m_rootSignatures = {};
    std::list<EfgTextureInternal*> m_renderTargets = {};
    std::list<EfgBufferInternal*> m_indexBuffers = {};
    std::list<EfgBufferInternal*> m_vertexBuffers = {};
    std::list<EfgPSOInternal*> m_pipelineStates = {};

    EfgPSOInternal* m_boundPSO = {};
    EfgVertexBuffer* m_boundVertexBuffer = {};
    EfgIndexBuffer* m_boundIndexBuffer = {};
    const EfgTexture* m_boundTexture = nullptr;

    // Synchronization objects.
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
};

XMMATRIX efgCreateTransformMatrix(XMFLOAT3 translation, XMFLOAT3 rotation, XMFLOAT3 scale);

