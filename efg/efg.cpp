#include "efg.h"
#include "efg_exception.h"
#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../../tinyobjloader/tiny_obj_loader.h"

XMMATRIX efgCreateTransformMatrix(XMFLOAT3 translation, XMFLOAT3 rotation, XMFLOAT3 scale)
{
    XMVECTOR rotationRadians = XMVectorSet(XMConvertToRadians(rotation.x), XMConvertToRadians(rotation.y), XMConvertToRadians(rotation.z), 0.0f);
    XMMATRIX translationMatrix = XMMatrixTranslation(translation.x, translation.y, translation.z);
    XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rotationRadians.m128_f32[0], rotationRadians.m128_f32[1], rotationRadians.m128_f32[2]);
    XMMATRIX scalingMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX modelMatrix = scalingMatrix * rotationMatrix * translationMatrix;

    return modelMatrix;
}

void EfgContext::initialize(HWND window)
{
	window_ = window;
    RECT window_rect = {};
    GetClientRect(window_, &window_rect);
    windowWidth = window_rect.right - window_rect.left;
    windowHeight = window_rect.bottom - window_rect.top;

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(windowWidth), static_cast<LONG>(windowHeight));

    m_aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);

    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    LoadPipeline();
    LoadAssets();
}

std::wstring EfgContext::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}

_Use_decl_annotations_
void EfgContext::GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if(adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    
    *ppAdapter = adapter.Detach();
}

void EfgContext::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    EFG_D3D_TRY(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        EFG_D3D_TRY(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        EFG_D3D_TRY(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        EFG_D3D_TRY(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    EFG_D3D_TRY(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = windowWidth;
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    EFG_D3D_TRY(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        window_,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    EFG_D3D_TRY(factory->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER));

    EFG_D3D_TRY(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_backBufferHeap = CreateDescriptorHeap(FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_rtvHeap = CreateDescriptorHeap(1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;

    EFG_D3D_TRY(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backBufferHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            EFG_D3D_TRY(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    EFG_D3D_TRY(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

EfgTexture EfgContext::CreateDepthBuffer()
{
    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);

    D3D12_RESOURCE_DESC depthStencilDesc = {};
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Width = 1920;
    depthStencilDesc.Height = 1080;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;
    
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    
    textureInternal->dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    textureInternal->dsvHandle.Offset(m_dsvDescriptorCount, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
    m_dsvDescriptorCount++;

    m_device->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, textureInternal->dsvHandle);

    return texture;
}

EfgTexture EfgContext::CreateShadowMap(uint32_t width, uint32_t height)
{
    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);

    D3D12_RESOURCE_DESC shadowMapDesc = {};
    shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    shadowMapDesc.Width = width;
    shadowMapDesc.Height = height;
    shadowMapDesc.DepthOrArraySize = 1;
    shadowMapDesc.MipLevels = 1;
    shadowMapDesc.Format = DXGI_FORMAT_D32_FLOAT;
    shadowMapDesc.SampleDesc.Count = 1;
    shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    
    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &shadowMapDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&textureInternal->resource)
    );

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    textureInternal->dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    textureInternal->dsvHandle.Offset(m_dsvDescriptorCount, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
    m_dsvDescriptorCount++;
    
    m_device->CreateDepthStencilView(textureInternal->resource.Get(), &dsvDesc, textureInternal->dsvHandle);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        textureInternal->resource.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    ));

    return texture;
}

EfgTexture EfgContext::CreateColorBuffer(uint32_t width, uint32_t height)
{
    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);

    D3D12_RESOURCE_DESC colorBufferDesc = {};
    colorBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    colorBufferDesc.Width = width;
    colorBufferDesc.Height = height;
    colorBufferDesc.DepthOrArraySize = 1;
    colorBufferDesc.MipLevels = 1;
    colorBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorBufferDesc.SampleDesc.Count = 1;
    colorBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 0.0f;
    
    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &colorBufferDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&textureInternal->resource)
    );

    textureInternal->rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    textureInternal->rtvHandle.Offset(m_rtvDescriptorCount, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
    m_rtvDescriptorCount++;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    m_device->CreateRenderTargetView(textureInternal->resource.Get(), &rtvDesc, textureInternal->rtvHandle);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        textureInternal->resource.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    ));

    return texture;
}

void EfgContext::Copy2DTextureToBackbuffer(EfgTexture texture)
{
    EfgTextureInternal* textureInternal = reinterpret_cast<EfgTextureInternal*>(texture.handle);
    ComPtr<ID3D12Resource> backBuffer = m_renderTargets[m_frameIndex].Get();
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        textureInternal->resource.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_SOURCE));
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST));
    m_commandList->CopyResource(backBuffer.Get(), textureInternal->resource.Get());

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        textureInternal->resource.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET));
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET));
}

ComPtr<ID3D12DescriptorHeap> EfgContext::CreateDescriptorHeap(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    ComPtr<ID3D12DescriptorHeap> heap = {};
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    EFG_D3D_TRY(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));

    return heap;
}

void EfgContext::LoadAssets()
{
    // Create the command list.
    EFG_D3D_TRY(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    EFG_D3D_TRY(m_commandList->Close());

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        EFG_D3D_TRY(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            EFG_D3D_TRY(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void EfgContext::Frame()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    EFG_D3D_TRY(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    EFG_D3D_TRY(m_commandList->Reset(m_commandAllocator.Get(), m_boundPSO.pipelineState.Get()));

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


    //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_backBufferHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    //D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    //m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvSrvHeap.Get(), m_samplerHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // Record commands.
    //const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    //m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    //m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void EfgContext::Render()
{
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ExecuteCommandList();

    // Present the frame.
    EFG_D3D_TRY(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void EfgContext::ExecuteCommandList()
{
    // Execute the command list.
    EFG_D3D_TRY(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

EfgResult EfgContext::CreateCbvSrvDescriptorHeap(uint32_t numDescriptors)
{
    if (numDescriptors > 0)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        EFG_D3D_TRY(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_cbvSrvHeap)));

        m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    return EfgResult_NoError;
}

void EfgContext::CreateSamplerDescriptorHeap(uint32_t samplerCount)
{
    if (samplerCount > 0)
    {
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = samplerCount;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        EFG_D3D_TRY(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));

        m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
}

EfgResult EfgContext::CreateConstantBufferView(EfgConstantBuffer* buffer, uint32_t heapOffset)
{
    if (!m_cbvSrvHeap)
    {
        EFG_SHOW_ERROR("Cannot create CBV. Heap was not created.");
        return EfgResult_InvalidOperation;
    }
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = buffer->m_bufferResource->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = buffer->alignmentSize;
    buffer->heapOffset = heapOffset;
    buffer->cbvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    buffer->cbvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_device->CreateConstantBufferView(&cbvDesc, buffer->cbvHandle);
    return EfgResult_NoError;
}

EfgResult EfgContext::CreateStructuredBufferView(EfgStructuredBuffer* buffer, uint32_t heapOffset)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = buffer->count;
    srvDesc.Buffer.StructureByteStride = (UINT)buffer->stride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    buffer->heapOffset = heapOffset;
    buffer->srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    buffer->srvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_device->CreateShaderResourceView(buffer->m_bufferResource.Get(), &srvDesc, buffer->srvHandle);
    return EfgResult_NoError;
}

void EfgContext::CreateTextureView(EfgTextureInternal* texture, uint32_t heapOffset)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->resource.Get()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texture->resource.Get()->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    texture->heapOffset = heapOffset;
    texture->srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    texture->srvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    m_device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, texture->srvHandle);
}

void EfgContext::CreateTextureCubeView(EfgTextureInternal* texture, uint32_t heapOffset)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->resource.Get()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    texture->heapOffset = heapOffset;
    texture->srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    texture->srvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    
    m_device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, texture->srvHandle);
}

void EfgContext::CommitSampler(EfgSamplerInternal* sampler, uint32_t heapOffset)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
    samplerHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER));
    m_device->CreateSampler(&sampler->desc, samplerHandle);
    sampler->heapOffset = heapOffset;
}

EfgResult EfgContext::CommitShaderResources()
{
    CreateCbvSrvDescriptorHeap(m_cbvDescriptorCount + m_srvDescriptorCount + m_textureCount +m_textureCubeCount);
    CreateSamplerDescriptorHeap(m_samplerCount);

    uint32_t heapOffset = 0;
    for (EfgConstantBuffer* buffer : m_constantBuffers) {
        CreateConstantBufferView(buffer, heapOffset);
        heapOffset++;
    }

    for (EfgStructuredBuffer* buffer : m_structuredBuffers) {
        CreateStructuredBufferView(buffer, heapOffset);
        heapOffset++;
    }

    for (EfgTextureInternal* texture : m_textures) {
        CreateTextureView(texture, heapOffset);
        heapOffset++;
    }

    for (EfgTextureInternal* texture : m_textureCubes) {
        CreateTextureCubeView(texture, heapOffset);
        heapOffset++;
    }

    heapOffset = 0;
    for (EfgSamplerInternal* sampler : m_samplers) {
        CommitSampler(sampler, heapOffset);
        heapOffset++;
    }
    return EfgResult_NoError;
}

void EfgContext::DrawInstanced(uint32_t vertexCount)
{
    m_commandList->IASetVertexBuffers(0, 1, &m_boundVertexBuffer.view);
    m_commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void EfgContext::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount)
{
    m_commandList->IASetVertexBuffers(0, 1, &m_boundVertexBuffer.view);
    m_commandList->IASetIndexBuffer(&m_boundIndexBuffer.view);
    m_commandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
}

void EfgContext::Destroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void EfgContext::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    EFG_D3D_TRY(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        EFG_D3D_TRY(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

ComPtr<ID3D12Resource> EfgContext::CreateBufferResource(EFG_CPU_ACCESS cpuAccess, UINT size)
{
    ComPtr<ID3D12Resource> resource = {};
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

    switch (cpuAccess)
    {
    case EFG_CPU_WRITE:
        heapType = D3D12_HEAP_TYPE_UPLOAD;
        resourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        break;
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    EFG_D3D_TRY(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        resourceState,
        nullptr,
        IID_PPV_ARGS(&resource)
    ));

    return resource;
}

void EfgContext::CreateBuffer(void const* data, EfgBufferInternal& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState)
{
    ComPtr<ID3D12Resource> uploadBuffer = CreateBufferResource(EFG_CPU_WRITE, buffer.alignmentSize);
    
    void* mappedData;
    CD3DX12_RANGE readRange(0, 0);
    EFG_D3D_TRY(uploadBuffer->Map(0, &readRange, &mappedData));

    // Debug print before copying
    //const int* intData = reinterpret_cast<const int*>(data);
    //std::cout << "Data before copy: " << intData[0] << ", " << intData[1] << std::endl;

    memcpy(mappedData, data, buffer.size);

    // Debug print after copying
    //int* mappedIntData = reinterpret_cast<int*>(mappedData);
    //std::cout << "Mapped data after copy: " << mappedIntData[0] << ", " << mappedIntData[1] << std::endl;

    switch(cpuAccess)
    {
    case EFG_CPU_NONE:
        uploadBuffer->Unmap(0, nullptr);
        buffer.m_bufferResource = CreateBufferResource(cpuAccess, buffer.size);
        CopyBuffer(buffer.m_bufferResource, uploadBuffer, buffer.alignmentSize, D3D12_RESOURCE_STATE_COMMON, finalState);
        break;
    case EFG_CPU_WRITE:
        CD3DX12_RANGE writeRange(0, buffer.alignmentSize);
        uploadBuffer->Unmap(0, &writeRange);
        buffer.m_bufferResource = uploadBuffer;
        break;
    }
}

void EfgContext::TransitionResourceState(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES finalState)
{
    if (currentState != finalState)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource.Get();
        barrier.Transition.StateBefore = currentState;
        barrier.Transition.StateAfter = finalState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        
        m_commandList->ResourceBarrier(1, &barrier);
    }
}

void EfgContext::CopyBuffer(ComPtr<ID3D12Resource> dest, ComPtr<ID3D12Resource> src, UINT size, D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES finalState)
{
    ResetCommandList();
    
    TransitionResourceState(dest, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    m_commandList->CopyBufferRegion(dest.Get(), 0, src.Get(), 0, size);
    TransitionResourceState(dest, D3D12_RESOURCE_STATE_COPY_DEST, finalState);

    ExecuteCommandList();
    WaitForGpu();
}


EfgVertexBuffer EfgContext::CreateVertexBuffer(void const* data, UINT size)
{
    EfgVertexBuffer buffer = { };

    buffer.size = size;
    buffer.alignmentSize = size;
    buffer.type = EFG_VERTEX_BUFFER;
    CreateBuffer(data, buffer, EFG_CPU_NONE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    buffer.view.BufferLocation = buffer.m_bufferResource->GetGPUVirtualAddress();
    buffer.view.StrideInBytes = sizeof(Vertex);
    buffer.view.SizeInBytes = buffer.size;

    return buffer;
}

EfgIndexBuffer EfgContext::CreateIndexBuffer(void const* data, UINT size)
{
    EfgIndexBuffer buffer = { };

    buffer.size = size;
    buffer.alignmentSize = size;
    buffer.type = EFG_INDEX_BUFFER;
    CreateBuffer(data, buffer, EFG_CPU_NONE, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    buffer.view.BufferLocation = buffer.m_bufferResource->GetGPUVirtualAddress();
    buffer.view.Format = DXGI_FORMAT_R32_UINT;
    buffer.view.SizeInBytes = buffer.size;

    return buffer;
}

EfgBuffer EfgContext::CreateConstantBuffer(void const* data, UINT size)
{
    EfgBuffer buffer = {};
    EfgConstantBuffer* bufferInternal = new EfgConstantBuffer();

    buffer.handle = reinterpret_cast<uint64_t>(bufferInternal);
    bufferInternal->size = size;
    bufferInternal->alignmentSize = (size + 255) & ~255;
    bufferInternal->type = EFG_CONSTANT_BUFFER;
    CreateBuffer(data, *bufferInternal, EFG_CPU_WRITE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_constantBuffers.push_back(bufferInternal);
    m_cbvDescriptorCount++;

    return buffer;
}

EfgBuffer EfgContext::CreateStructuredBuffer(void const* data, UINT size, uint32_t count, size_t stride)
{
    EfgBuffer buffer = {};
    EfgStructuredBuffer* bufferInternal = new EfgStructuredBuffer();

    buffer.handle = reinterpret_cast<uint64_t>(bufferInternal);
    bufferInternal->size = size;
    bufferInternal->alignmentSize = size;
    bufferInternal->type = EFG_STRUCTURED_BUFFER;
    bufferInternal->count = count;
    bufferInternal->stride = stride;
    CreateBuffer(data, *bufferInternal, EFG_CPU_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    m_structuredBuffers.push_back(bufferInternal);
    m_srvDescriptorCount++;

    return buffer;
}

EfgTexture EfgContext::CreateTexture2D()
{
    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);

    return texture;
}

EfgTexture EfgContext::CreateTexture2DFromFile(const wchar_t* filename)
{
    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);
    ResourceUploadBatch resourceUpload(m_device.Get());
    resourceUpload.Begin();
    EFG_D3D_TRY(CreateWICTextureFromFile(m_device.Get(), resourceUpload, filename, textureInternal->resource.ReleaseAndGetAddressOf()));
    auto uploadResourcesFinished = resourceUpload.End(m_commandQueue.Get());
    uploadResourcesFinished.wait();
    texture.index = m_textureCount;
    m_textures.push_back(textureInternal);
    m_textureCount++;
    return texture;
}

EfgTexture EfgContext::CreateTextureCube(const std::vector<std::wstring>& filenames)
{
    // Ensure there are six filenames provided
    if (filenames.size() != 6) throw std::runtime_error("Six filenames required for a texture cube");

    EfgTexture texture = {};
    EfgTextureInternal* textureInternal = new EfgTextureInternal();
    texture.handle = reinterpret_cast<uint64_t>(textureInternal);
    ComPtr<ID3D12Resource> textureResources[6];

    // Describe the cube texture
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = 1024; // Example width, should match your texture's dimensions
    textureDesc.Height = 1024; // Example height, should match your texture's dimensions
    textureDesc.DepthOrArraySize = 6; // 6 faces for the cube map
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create the cube texture resource
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    EFG_D3D_TRY(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&textureInternal->resource)));

    ResourceUploadBatch resourceUpload(m_device.Get());
    resourceUpload.Begin();

    for (int i = 0; i < 6; ++i) {
        CreateWICTextureFromFileEx(
            m_device.Get(),
            resourceUpload,
            filenames[i].c_str(),
            0,
            D3D12_RESOURCE_FLAG_NONE,
            WIC_LOADER_FORCE_RGBA32,
            textureResources[i].ReleaseAndGetAddressOf()
        );
    }

    auto uploadResourcesFinished = resourceUpload.End(m_commandQueue.Get());
    uploadResourcesFinished.wait();

    ResetCommandList();

    for (int i = 0; i < 6; ++i) {
        // Transition the resource state to COPY_SOURCE
        CD3DX12_RESOURCE_BARRIER transitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            textureResources[i].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        );
        m_commandList->ResourceBarrier(1, &transitionBarrier);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = textureInternal->resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = D3D12CalcSubresource(0, i, 0, 1, 6);
    
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = textureResources[i].Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;
    
        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureInternal->resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    m_commandList->ResourceBarrier(1, &barrier);

    ExecuteCommandList();
    WaitForGpu();

    texture.index = m_textureCount;
    m_textureCubes.push_back(textureInternal);
    m_textureCubeCount++;
    return texture;
}

EfgSampler EfgContext::CreateSampler()
{
    EfgSampler sampler = {};
    EfgSamplerInternal* samplerInternal = new EfgSamplerInternal();
    sampler.handle = reinterpret_cast<uint64_t>(samplerInternal);
    samplerInternal->desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerInternal->desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerInternal->desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerInternal->desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerInternal->desc.MipLODBias = 0.0f;
    samplerInternal->desc.MaxAnisotropy = 1;
    samplerInternal->desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerInternal->desc.BorderColor[0] = 1.0f;
    samplerInternal->desc.BorderColor[1] = 1.0f;
    samplerInternal->desc.BorderColor[2] = 1.0f;
    samplerInternal->desc.BorderColor[3] = 1.0f;
    samplerInternal->desc.MinLOD = 0.0f;
    samplerInternal->desc.MaxLOD = D3D12_FLOAT32_MAX;
    m_samplers.push_back(samplerInternal);
    m_samplerCount++;

    return sampler;
}

void EfgContext::UpdateConstantBuffer(EfgBuffer& buffer, void const* data, UINT size)
{
    EfgConstantBuffer* bufferInternal = reinterpret_cast<EfgConstantBuffer*>(buffer.handle);
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    D3D12_RANGE writeRange = { 0, size };
    bufferInternal->m_bufferResource->Map(0, &readRange, &mappedData);
    if (mappedData)
        memcpy(mappedData, data, (size_t)size);
    bufferInternal->m_bufferResource->Unmap(0, &writeRange);
}

void EfgContext::UpdateStructuredBuffer(EfgBuffer& buffer, void const* data, UINT size)
{
    EfgStructuredBuffer* bufferInternal = reinterpret_cast<EfgStructuredBuffer*>(buffer.handle);
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    D3D12_RANGE writeRange = { 0, size };
    bufferInternal->m_bufferResource->Map(0, &readRange, &mappedData);
    if (mappedData)
        memcpy(mappedData, data, (size_t)size);
    bufferInternal->m_bufferResource->Unmap(0, &writeRange);
}

void EfgContext::WaitForGpu()
{
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        if (eventHandle == NULL)
            EFG_D3D_TRY(HRESULT_FROM_WIN32(GetLastError()));
        EFG_D3D_TRY(m_fence->SetEventOnCompletion(m_fenceValue, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void EfgContext::ResetCommandList()
{
    EFG_D3D_TRY(m_commandAllocator->Reset());
    EFG_D3D_TRY(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
}

EfgShader EfgContext::CreateShader(LPCWSTR fileName, LPCSTR target, LPCSTR entryPoint)
{
    EfgShader shader = {};
    shader.source = GetAssetFullPath(fileName);
    CompileShader(shader, entryPoint, target);
    return shader;
}

EfgPSO EfgContext::CreateGraphicsPipelineState(EfgProgram program, EfgRootSignature& rootSignature)
{
    EfgPSO pso = {};
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    pso.rootSignature = rootSignature.Get();

    // Define the rasterizer state description
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // Fill solid polygons
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;  // Cull back-facing polygons
    rasterizerDesc.FrontCounterClockwise = FALSE;    // Counter-clockwise vertices define the front
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;           // Enable depth clipping
    rasterizerDesc.MultisampleEnable = FALSE;        // Disable multisampling
    rasterizerDesc.AntialiasedLineEnable = FALSE;    // Disable antialiased lines
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Describe and create the graphics pipeline state object (PSO).
    pso.desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    pso.desc.pRootSignature = rootSignature.Get().Get();
    pso.desc.VS = CD3DX12_SHADER_BYTECODE(program.vertexShader.byteCode.Get());
    pso.desc.PS = CD3DX12_SHADER_BYTECODE(program.pixelShader.byteCode.Get());
    pso.desc.RasterizerState = rasterizerDesc;
    pso.desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.desc.DepthStencilState.DepthEnable = TRUE;
    pso.desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.desc.DepthStencilState.StencilEnable = FALSE;
    pso.desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.desc.SampleMask = UINT_MAX;
    pso.desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.desc.NumRenderTargets = 1;
    pso.desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.desc.SampleDesc.Count = 1;
    EFG_D3D_TRY(m_device->CreateGraphicsPipelineState(&pso.desc, IID_PPV_ARGS(&pso.pipelineState)));
    return pso;
}

EfgPSO EfgContext::CreateShadowMapPSO(EfgProgram program, EfgRootSignature rootSignature)
{
    EfgPSO pso = {};
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        //{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    ZeroMemory(&pso.desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    pso.rootSignature = rootSignature.Get();
    pso.desc.pRootSignature = rootSignature.Get().Get();
    pso.desc.VS = CD3DX12_SHADER_BYTECODE(program.vertexShader.byteCode.Get());
    pso.desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.desc.SampleMask = UINT_MAX;
    pso.desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.desc.RasterizerState.DepthBias = 1000;
    pso.desc.RasterizerState.DepthBiasClamp = 0.0f;
    pso.desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    pso.desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.desc.DepthStencilState.DepthEnable = TRUE;
    pso.desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.desc.NumRenderTargets = 0;
    pso.desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    pso.desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.desc.SampleDesc.Count = 1;
    pso.desc.SampleDesc.Quality = 0;
    EFG_D3D_TRY(m_device->CreateGraphicsPipelineState(&pso.desc, IID_PPV_ARGS(&pso.pipelineState)));
    return pso;
}

void EfgContext::SetPipelineState(EfgPSO pso)
{
    m_commandList->SetGraphicsRootSignature(pso.rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->SetPipelineState(pso.pipelineState.Get());
    m_boundPSO = pso;
}

void EfgContext::SetRenderTarget(EfgTexture texture, EfgTexture* depthStencil)
{
    EfgTextureInternal* textureInternal = reinterpret_cast<EfgTextureInternal*>(texture.handle);
    if(textureInternal->dsvHandle.ptr != 0)
        m_commandList->OMSetRenderTargets(0, nullptr, false, &textureInternal->dsvHandle);
    if (textureInternal->rtvHandle.ptr != 0)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE* handle = nullptr;
        if (depthStencil != nullptr)
        {
            EfgTextureInternal* depthStencilInternal = nullptr;
            depthStencilInternal = reinterpret_cast<EfgTextureInternal*>(depthStencil->handle);
            handle = &depthStencilInternal->dsvHandle;
        }
        m_commandList->OMSetRenderTargets(1, &textureInternal->rtvHandle, FALSE, handle);
    }
}

void EfgContext::ClearDepthStencilView(EfgTexture texture)
{
    EfgTextureInternal* textureInternal = reinterpret_cast<EfgTextureInternal*>(texture.handle);
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = {};
    if (textureInternal->dsvHandle.ptr != 0)
        handle = textureInternal->dsvHandle;
    if (textureInternal->rtvHandle.ptr != 0)
        handle = textureInternal->rtvHandle;
    m_commandList->ClearDepthStencilView(handle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f, 0, 0, nullptr
    );
}

void EfgContext::ClearRenderTargetView(EfgTexture texture)
{
    EfgTextureInternal* textureInternal = reinterpret_cast<EfgTextureInternal*>(texture.handle);
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_commandList->ClearRenderTargetView(textureInternal->rtvHandle, clearColor, 0, nullptr);
}

void EfgContext::BindVertexBuffer(EfgVertexBuffer buffer)
{
    m_boundVertexBuffer = buffer;
}

void EfgContext::BindIndexBuffer(EfgIndexBuffer buffer)
{
    m_boundIndexBuffer = buffer;
}

void EfgContext::Bind2DTexture(uint32_t index, const EfgTexture& texture)
{
    m_boundTexture = &texture;
    EfgTextureInternal* textureInternal = reinterpret_cast<EfgTextureInternal*>(texture.handle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), textureInternal->heapOffset, m_cbvSrvDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(index, gpuHandle);
}

void EfgContext::BindConstantBuffer(uint32_t index, const EfgBuffer& buffer)
{
    EfgBufferInternal* bufferInternal = reinterpret_cast<EfgBufferInternal*>(buffer.handle);
    m_commandList->SetGraphicsRootConstantBufferView(index, bufferInternal->m_bufferResource.Get()->GetGPUVirtualAddress());
}

void EfgContext::BindStructuredBuffer(uint32_t index, const EfgBuffer& buffer)
{
    EfgBufferInternal* bufferInternal = reinterpret_cast<EfgBufferInternal*>(buffer.handle);
    m_commandList->SetGraphicsRootShaderResourceView(index, bufferInternal->m_bufferResource.Get()->GetGPUVirtualAddress());
}

void EfgContext::CompileShader(EfgShader& shader, LPCSTR entryPoint, LPCSTR target)
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        shader.source.c_str(),
        nullptr,           // Optional macros
        nullptr,           // Optional include files
        entryPoint,        // Entry point for shader
        target,            // Shader model (vs_5_0, ps_5_0, etc.)
        compileFlags,      // Compile options
        0,                 // Effect compile options
        &shaderBlob,       // Compiled shader
        &errorBlob);       // Errors

    if (FAILED(hr))
    {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        EFG_D3D_TRY(hr);
    }
    
    shader.byteCode = shaderBlob;
}


void EfgContext::CheckD3DErrors()
{
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device.As(&infoQueue)))
    {
        UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < messageCount; ++i)
        {
            SIZE_T messageLength = 0;
            infoQueue->GetMessage(i, nullptr, &messageLength);
            D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(messageLength);
            if (message)
            {
                infoQueue->GetMessage(i, message, &messageLength);
                std::cerr << "D3D12 Error: " << message->pDescription << std::endl;
                free(message);
            }
        }
        infoQueue->ClearStoredMessages();
    }
}

D3D12_DESCRIPTOR_RANGE EfgDescriptorRange::Commit(bool useOffset = false)
{
    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    switch (rangeType)
    {
    case efgRange_CBV:
        descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        break;
    case efgRange_SRV:
        descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        break;
    case efgRange_SAMPLER:
        descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        break;
    }
    descriptorRange.NumDescriptors = numDescriptors;
    descriptorRange.BaseShaderRegister = baseShaderRegister;
    descriptorRange.RegisterSpace = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = useOffset ? offset : D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    return descriptorRange;
}

D3D12_ROOT_PARAMETER EfgRootParameter::Commit(ShaderRegisters& registers)
{
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = type;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    switch (type)
    {
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        rootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
        rootParameter.DescriptorTable.pDescriptorRanges = ranges.data();
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            switch (ranges[i].RangeType)
            {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                ranges[i].BaseShaderRegister = registers.CBV;
                registers.CBV += ranges.data()->NumDescriptors;
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                ranges[i].BaseShaderRegister = registers.SRV;
                registers.SRV+= ranges.data()->NumDescriptors;
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                ranges[i].BaseShaderRegister = registers.UAV;
                registers.UAV += ranges.data()->NumDescriptors;
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                ranges[i].BaseShaderRegister = registers.SAMPLER;
                registers.SAMPLER += ranges.data()->NumDescriptors;
                break;
            }
        }
        break;
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        break;
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
        rootParameter.Descriptor.ShaderRegister = registers.CBV;
        registers.CBV++;
        break;
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
        rootParameter.Descriptor.ShaderRegister = registers.SRV;
        registers.SRV++;
        break;
    case D3D12_ROOT_PARAMETER_TYPE_UAV:
        rootParameter.Descriptor.ShaderRegister = registers.UAV;
        registers.UAV++;
        break;
    }
    return rootParameter;
}

ComPtr<ID3DBlob> EfgRootSignature::Serialize()
{
    rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
    rootSignatureDesc.pParameters = rootParameters.data();
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSignature;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw EfgException(hr);
    }
    return serializedRootSignature;
}

void EfgContext::CreateRootSignature(EfgRootSignature& rootSignature)
{
    ComPtr<ID3DBlob> serializedRootSignature = rootSignature.Serialize();
    ThrowIfFailed(m_device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature.Get())));
}

void EfgContext::BindRootDescriptorTable(EfgRootSignature& rootSignature)
{
    uint32_t offset = 0;
    ComPtr<ID3D12DescriptorHeap> heap = {};
    for (int i = 0; i < rootSignature.rootParameters.size(); i++)
    {
        if (rootSignature.parameterData[i].conditionalBind)
            continue;
        offset = rootSignature.parameterData[i].offset;
        UINT descriptorSize = 0;
        switch (rootSignature.rootParameters[i].DescriptorTable.pDescriptorRanges->RangeType)
        {
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            heap = m_cbvSrvHeap;
            descriptorSize = m_cbvSrvDescriptorSize;
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            offset = 0;
            heap = m_samplerHeap;
            descriptorSize = m_samplerDescriptorSize;
            break;
        }
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(heap->GetGPUDescriptorHandleForHeapStart(), offset, descriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(i, gpuHandle);
    }
}

EfgImportMesh EfgContext::LoadFromObj(const char* basePath, const char* file)
{
    EfgImportMesh mesh;
    tinyobj::ObjReaderConfig readerConfig;
    if(basePath != nullptr)
        readerConfig.mtl_search_path = basePath;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(file, readerConfig))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    if (!reader.Warning().empty())
    {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto shapes = reader.GetShapes();
    auto materials = reader.GetMaterials();

    mesh.constants.isInstanced = false;
    mesh.constants.useTransform = false;
    mesh.constantsBuffer = CreateConstantBuffer<ObjectConstants>(&mesh.constants, 1);

    for (size_t m = 0; m < materials.size(); m++)
    {
        EfgMaterialBuffer material;
        EfgMaterialTextures textures;
        tinyobj::material_t importMat = materials[m];
        material.ambient= XMFLOAT4(importMat.ambient[0], importMat.ambient[1], importMat.ambient[2], 0.0f);
        material.diffuse = XMFLOAT4(importMat.diffuse[0], importMat.diffuse[1], importMat.diffuse[2], 0.0f);
        material.specular = XMFLOAT4(importMat.specular[0],importMat.specular[1], importMat.specular[2], 0.0f);
        material.emission = XMFLOAT4(importMat.emission[0], importMat.emission[1], importMat.emission[2], 0.0f);
        material.transmittance = XMFLOAT4(importMat.transmittance[0], importMat.transmittance[1], importMat.transmittance[2], 0.0f);
        material.shininess = importMat.shininess;
        material.roughness = importMat.roughness;
        material.metallic = importMat.metallic;
        material.ior = importMat.ior;
        material.dissolve = importMat.dissolve;
        material.clearcoat = importMat.clearcoat_thickness;
        material.clearcoat_roughness = importMat.clearcoat_roughness;
        if (!materials[m].diffuse_texname.empty())
        {
            material.diffuseMapFlag = 1;
            std::string texPath;
            if (basePath != nullptr)
                texPath = std::string(basePath) + "\\" + materials[m].diffuse_texname;
            else
                texPath = materials[m].diffuse_texname;
            std::wstring w_texPath(texPath.begin(), texPath.end());
            textures.diffuse_map = CreateTexture2DFromFile(w_texPath.c_str());
        }

        EfgBuffer materialBuffer = CreateConstantBuffer<EfgMaterialBuffer>(&material, 1);
        mesh.materialBuffers.push_back(materialBuffer);
        mesh.textures.push_back(textures);
    }

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
      // Loop over faces(polygon)
      size_t index_offset = 0;
      for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
        size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
        // Loop over vertices in the face.
        for (size_t v = 0; v < fv; v++) {
          Vertex vertex;
          // access to vertex
          tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
          vertex.position.x = attrib.vertices[3*size_t(idx.vertex_index)+0];
          vertex.position.y = attrib.vertices[3*size_t(idx.vertex_index)+1];
          vertex.position.z = attrib.vertices[3*size_t(idx.vertex_index)+2];
    
          // Check if `normal_index` is zero or positive. negative = no normal data
          if (idx.normal_index >= 0) {
            vertex.normal.x = attrib.normals[3*size_t(idx.normal_index)+0];
            vertex.normal.y = attrib.normals[3*size_t(idx.normal_index)+1];
            vertex.normal.z = attrib.normals[3*size_t(idx.normal_index)+2];
          }
    
          // Check if `texcoord_index` is zero or positive. negative = no texcoord data
          if (idx.texcoord_index >= 0) {
            vertex.uv.x = attrib.texcoords[2*size_t(idx.texcoord_index)+0];
            vertex.uv.y = attrib.texcoords[2*size_t(idx.texcoord_index)+1];
          }
    
          // Optional: vertex colors
          // tinyobj::real_t red   = attrib.colors[3*size_t(idx.vertex_index)+0];
          // tinyobj::real_t green = attrib.colors[3*size_t(idx.vertex_index)+1];
          // tinyobj::real_t blue  = attrib.colors[3*size_t(idx.vertex_index)+2];
          mesh.materialBatches[shapes[s].mesh.material_ids[f]].vertices.push_back(vertex);
          mesh.materialBatches[shapes[s].mesh.material_ids[f]].indices.push_back(mesh.materialBatches[shapes[s].mesh.material_ids[f]].indices.size());
        }
        index_offset += fv;
      }

      for (size_t m = 0; m < mesh.materialBatches.size(); m++)
      {
        if (mesh.materialBatches[m].vertices.size() > 0)
        {
            mesh.materialBatches[m].vertexBuffer = CreateVertexBuffer<Vertex>(mesh.materialBatches[m].vertices.data(), mesh.materialBatches[m].vertices.size());
            mesh.materialBatches[m].indexBuffer = CreateIndexBuffer<uint32_t>(mesh.materialBatches[m].indices.data(), mesh.materialBatches[m].indices.size());
            mesh.materialBatches[m].indexCount = mesh.materialBatches[m].indices.size();
        }
      }
    }

    return mesh;
}
