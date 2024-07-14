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

struct EfgBuffer
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

class EfgContext
{
public:
	friend class EfgInternal;
private:
	uint64_t handle;
};

class EfgInternal
{
public:
	EfgInternal(EfgContext& efg) { efg.handle = reinterpret_cast<uint64_t>(this); };
	void initialize(HWND window);
	static inline EfgInternal* GetEfg(EfgContext& context);
    EfgVertexBuffer CreateVertexBuffer(void const* data, UINT size);
    EfgIndexBuffer CreateIndexBuffer(void const* data, UINT size);
    void CreateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size);
    void CreateStructuredBuffer(EfgStructuredBuffer& buffer, void const* data, UINT size, uint32_t numElements, size_t stride);
    void updateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size);
    void BindVertexBuffer(EfgVertexBuffer buffer);
    void BindIndexBuffer(EfgIndexBuffer buffer);
    EfgResult CommitShaderResources();
    EfgProgram CreateProgram(LPCWSTR fileName);
    EfgPSO CreateGraphicsPipelineState(EfgProgram program);
    void SetPipelineState(EfgPSO pso);
    void DrawInstanced(uint32_t vertexCount);
    void DrawIndexedInstanced(uint32_t indexCount);
    void Render();
    void Destroy();
    void CheckD3DErrors();

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
    EfgResult CreateRootSignature(uint32_t numCbv, uint32_t numSrv);
    void bindDescriptorHeaps();

    void ResetCommandList();
    void ExecuteCommandList();
    void WaitForGpu();

    void CreateBuffer(void const* data, EfgBuffer& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState);
    void CopyBuffer(ComPtr<ID3D12Resource> dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES current, D3D12_RESOURCE_STATES final);
    ComPtr<ID3D12Resource> CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size);
    void TransitionResourceState(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState);
    EfgResult CreateCBVDescriptorHeap(uint32_t numDescriptors);
    EfgResult CreateConstantBufferView(EfgConstantBuffer* buffer, uint32_t heapOffset);
    EfgResult CreateStructuredBufferView(EfgStructuredBuffer* buffer, uint32_t heapOffset);

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
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    UINT m_rtvDescriptorSize = 0;
    UINT m_cbvDescriptorSize = 0;
    UINT m_srvDescriptorSize = 0;
    uint32_t m_cbvDescriptorCount = 0;
    uint32_t m_srvDescriptorCount = 0;
    std::list<EfgConstantBuffer*> m_constantBuffers = {};
    std::list<EfgStructuredBuffer*> m_structuredBuffers = {};

    EfgPSO m_boundPSO = {};
    EfgVertexBuffer m_boundVertexBuffer = {};
    EfgIndexBuffer m_boundIndexBuffer = {};

    // Synchronization objects.
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
};

EfgContext efgCreateContext(HWND window);
EfgResult efgDestroyContext(EfgContext context);
EfgResult efgBindVertexBuffer(EfgContext context, EfgVertexBuffer buffer);
EfgResult efgBindIndexBuffer(EfgContext context, EfgIndexBuffer buffer);
EfgVertexBuffer efgCreateVertexBuffer(EfgContext context, void const* data, UINT size);
EfgIndexBuffer efgCreateIndexBuffer(EfgContext context, void const* data, UINT size);
EfgResult efgCreateConstantBuffer(EfgContext context, EfgConstantBuffer& buffer, void const* data, UINT size);
EfgResult efgCreateStructuredBuffer(EfgContext context, EfgStructuredBuffer& buffer, void const* data, UINT size, uint32_t count, size_t stride);
EfgResult efgUpdateConstantBuffer(EfgContext context, EfgConstantBuffer& buffer, void const* data, UINT size);
EfgResult efgCommitShaderResources(EfgContext context);
EfgProgram efgCreateProgram(EfgContext context, LPCWSTR fileName);
EfgPSO efgCreateGraphicsPipelineState(EfgContext context, EfgProgram program);
EfgResult efgSetPipelineState(EfgContext efg, EfgPSO pso);
EfgResult efgDrawInstanced(EfgContext efg, uint32_t vertexCount);
EfgResult efgDrawIndexedInstanced(EfgContext efg, uint32_t indexCount);
EfgResult efgRender(EfgContext efg);
XMMATRIX efgCreateTransformMatrix(XMFLOAT3 translation, XMFLOAT3 rotation, XMFLOAT3 scale);

template<typename TYPE>
EfgVertexBuffer efgCreateVertexBuffer(EfgContext context, void const* data, uint32_t count)
{
    EfgVertexBuffer buffer = efgCreateVertexBuffer(context, data, count * sizeof(TYPE));
    return buffer;
}

template<typename TYPE>
EfgIndexBuffer efgCreateIndexBuffer(EfgContext context, void const* data, uint32_t count)
{
    EfgIndexBuffer buffer = efgCreateIndexBuffer(context, data, count * sizeof(TYPE));
    return buffer;
}

template<typename TYPE>
void efgCreateConstantBuffer(EfgContext context, EfgConstantBuffer& buffer, void const* data, uint32_t count)
{
    efgCreateConstantBuffer(context, buffer, data, count * sizeof(TYPE));
}

template<typename TYPE>
void efgCreateStructuredBuffer(EfgContext context, EfgStructuredBuffer& buffer, void const* data, uint32_t count)
{
    size_t stride = sizeof(TYPE);
    efgCreateStructuredBuffer(context, buffer, data, count * sizeof(TYPE), count, stride);
}
