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
    EFG_CONSTANT_BUFFER
} EFG_BUFFER_TYPE;

typedef
enum EFG_CPU_ACCESS 
{
    EFG_CPU_NONE,
    EFG_CPU_READ,
    EFG_CPU_WRITE
} EFG_CPU_ACCESS;

struct EfgBuffer
{
    EFG_BUFFER_TYPE type;
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
    EfgConstantBuffer CreateConstantBuffer(void const* data, UINT size);
    void updateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size);
    EfgProgram CreateProgram(LPCWSTR fileName);
    EfgPSO CreateGraphicsPipelineState(EfgProgram program);
    void SetPipelineState(EfgPSO pso);
    void BindVertexBuffer(EfgVertexBuffer buffer);
    void BindIndexBuffer(EfgIndexBuffer buffer);
    void DrawInstanced(uint32_t vertexCount);
    void DrawIndexedInstanced(uint32_t indexCount);
    void Render();
    void createCBVDescriptorHeap(uint32_t numDescriptors);
    void Destroy();


private:
    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false
    );

    void CompileProgram(EfgProgram& program);
    void CreateRootSignature(uint32_t numDescriptors);
    void bindCBVDescriptorHeaps();
    void ResetCommandList();
    void ExecuteCommandList();
    void WaitForGpu();
    void CreateBuffer(void const* data, EfgBuffer& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState);
    void CopyBuffer(ComPtr<ID3D12Resource> dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES current, D3D12_RESOURCE_STATES final);
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
    ComPtr<ID3D12Resource> CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size);
    void TransitionResourceState(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState);

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
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    UINT m_rtvDescriptorSize = 0;
    UINT m_cbvDescriptorSize = 0;
    uint32_t m_cbvDescriptorCount = 0;

    EfgPSO m_boundPSO = {};
    EfgVertexBuffer m_boundVertexBuffer = {};
    EfgIndexBuffer m_boundIndexBuffer = {};


    // Synchronization objects.
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;

    std::wstring GetAssetFullPath(LPCWSTR assetName);
    void LoadPipeline();
    void LoadAssets();
    void WaitForPreviousFrame();
};


EfgContext efgCreateContext(HWND window);
void efgDestroyContext(EfgContext context);
void efgBindVertexBuffer(EfgContext context, EfgVertexBuffer buffer);
void efgBindIndexBuffer(EfgContext context, EfgIndexBuffer buffer);
EfgVertexBuffer efgCreateVertexBuffer(EfgContext context, void const* data, UINT size);
EfgIndexBuffer efgCreateIndexBuffer(EfgContext context, void const* data, UINT size);
EfgConstantBuffer efgCreateConstantBuffer(EfgContext context, void const* data, UINT size);
void efgUpdateConstantBuffer(EfgContext context, EfgConstantBuffer& buffer, void const* data, UINT size);
EfgProgram efgCreateProgram(EfgContext context, LPCWSTR fileName);
EfgPSO efgCreateGraphicsPipelineState(EfgContext context, EfgProgram program);
void efgSetPipelineState(EfgContext efg, EfgPSO pso);
void efgDrawInstanced(EfgContext efg, uint32_t vertexCount);
void efgDrawIndexedInstanced(EfgContext efg, uint32_t indexCount);
void efgRender(EfgContext efg);
void efgCreateCBVDescriptorHeap(EfgContext context, uint32_t numDescriptors);

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
EfgConstantBuffer efgCreateConstantBuffer(EfgContext context, void const* data, uint32_t count)
{
    EfgConstantBuffer buffer = efgCreateConstantBuffer(context, data, count * sizeof(TYPE));
    return buffer;
}
