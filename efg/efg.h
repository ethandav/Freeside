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

#include "../DirectX-Headers/include/directx/d3dx12.h"
#include "DXHelper.h"
#include "efg_window.h"

using Microsoft::WRL::ComPtr;

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

private:
    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false
    );

	HWND window_ = {};
    static const UINT FrameCount = 2;
    bool useWarpDevice = false;
    uint32_t windowWidth = 0;
    uint32_t windowHeight = 0;

    // Pipeline objects.
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize = 0;

    // Synchronization objects.
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = 0;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;

    void LoadPipeline();
    //void LoadAssets();
    //void PopulateCommandList();
    //void WaitForPreviousFrame();
};

EfgContext efgCreateContext(HWND window);
void efgDestroyContext(EfgContext context);
