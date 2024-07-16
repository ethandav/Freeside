#include "efg.h"
#include "efg_exception.h"
#include <iostream>

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

    m_rtvHeap = CreateDescriptorHeap(FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

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

EfgResult EfgContext::CreateRootSignature(uint32_t numCbv, uint32_t numSrv, uint32_t numSampler, uint32_t numTextures)
{
    std::vector<D3D12_ROOT_PARAMETER> rootParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
    descriptorRanges.reserve(4);

    // Define descriptor range for CBV if needed
    if (numCbv > 0)
    {
        D3D12_DESCRIPTOR_RANGE cbvDescriptorRange = {};
        cbvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        cbvDescriptorRange.NumDescriptors = numCbv;
        cbvDescriptorRange.BaseShaderRegister = 0;
        cbvDescriptorRange.RegisterSpace = 0;
        cbvDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        descriptorRanges.push_back(cbvDescriptorRange);

        D3D12_ROOT_PARAMETER cbvRootParameter = {};
        cbvRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        cbvRootParameter.DescriptorTable.NumDescriptorRanges = 1;
        cbvRootParameter.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
        cbvRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(cbvRootParameter);
    }

    // Define descriptor range for SRV if needed
    if (numSrv > 0)
    {
        D3D12_DESCRIPTOR_RANGE srvDescriptorRange = {};
        srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvDescriptorRange.NumDescriptors = numSrv;
        srvDescriptorRange.BaseShaderRegister = 0;
        srvDescriptorRange.RegisterSpace = 0;
        srvDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        descriptorRanges.push_back(srvDescriptorRange);

        D3D12_ROOT_PARAMETER srvRootParameter = {};
        srvRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        srvRootParameter.DescriptorTable.NumDescriptorRanges = 1;
        srvRootParameter.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
        srvRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(srvRootParameter);
    }

    if (numTextures > 0)
    {
        D3D12_DESCRIPTOR_RANGE texDescriptorRange = {};
        texDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texDescriptorRange.NumDescriptors = 1;
        texDescriptorRange.BaseShaderRegister = numSrv;
        texDescriptorRange.RegisterSpace = 0;
        texDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        descriptorRanges.push_back(texDescriptorRange);

        D3D12_ROOT_PARAMETER texRootParameter = {};
        texRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        texRootParameter.DescriptorTable.NumDescriptorRanges = 1;
        texRootParameter.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
        texRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(texRootParameter);
    }

    if (numSampler > 0)
    {
        D3D12_DESCRIPTOR_RANGE samplerDescriptorRange = {};
        samplerDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        samplerDescriptorRange.NumDescriptors = numSampler;
        samplerDescriptorRange.BaseShaderRegister = 0;
        samplerDescriptorRange.RegisterSpace = 0;
        samplerDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        descriptorRanges.push_back(samplerDescriptorRange);

        D3D12_ROOT_PARAMETER samplerRootParameter = {};
        samplerRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        samplerRootParameter.DescriptorTable.NumDescriptorRanges = 1;
        samplerRootParameter.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
        samplerRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(samplerRootParameter);
    }

    // Create the root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
    rootSignatureDesc.pParameters = rootParameters.data();
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize and create the root signature
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

    EFG_D3D_TRY(m_device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    return EfgResult_NoError;
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

    // Set necessary state.
    //m_commandList->SetGraphicsRootSignature(m_boundPSO.rootSignature.Get());
    m_commandList->SetGraphicsRootSignature(m_boundPSO.rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    m_commandList->SetPipelineState(m_boundPSO.pipelineState.Get());

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
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
    buffer->srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    buffer->srvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_device->CreateShaderResourceView(buffer->m_bufferResource.Get(), &srvDesc, buffer->srvHandle);
    return EfgResult_NoError;
}

void EfgContext::CreateTextureView(EfgTexture* texture, uint32_t heapOffset)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = texture->resource.Get()->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texture->resource.Get()->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    texture->srvHandle = m_cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
    texture->srvHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

    m_device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, texture->srvHandle);
}

void EfgContext::CommitSampler(EfgSampler* sampler, uint32_t heapOffset)
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
    samplerHandle.Offset(heapOffset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    m_device->CreateSampler(&sampler->desc, samplerHandle);
}

EfgResult EfgContext::CommitShaderResources()
{
    CreateCbvSrvDescriptorHeap(m_cbvDescriptorCount + m_srvDescriptorCount + m_textureCount);
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

    for (EfgTexture* texture : m_textures) {
        CreateTextureView(texture, heapOffset);
        heapOffset++;
    }

    heapOffset = 0;
    for (EfgSampler* sampler : m_samplers) {
        CommitSampler(sampler, heapOffset);
        heapOffset++;
    }
    return EfgResult_NoError;
}

void EfgContext::bindDescriptorHeaps()
{
    if (m_cbvSrvHeap)
    {
        ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvSrvHeap.Get(), m_samplerHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(0, cbvGpuHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), m_cbvDescriptorCount, m_cbvSrvDescriptorSize);
        m_commandList->SetGraphicsRootDescriptorTable(1, srvGpuHandle);

        if (m_boundTexture)
        {
            uint32_t offset = m_cbvDescriptorCount + m_srvDescriptorCount + m_boundTexture->index;
            CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_cbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), offset, m_cbvSrvDescriptorSize);
            m_commandList->SetGraphicsRootDescriptorTable(2, srvGpuHandle);
        }

        CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle(m_samplerHeap->GetGPUDescriptorHandleForHeapStart());
        m_commandList->SetGraphicsRootDescriptorTable(3, samplerHandle);
    }
}

void EfgContext::DrawInstanced(uint32_t vertexCount)
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    EFG_D3D_TRY(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    EFG_D3D_TRY(m_commandList->Reset(m_commandAllocator.Get(), m_boundPSO.pipelineState.Get()));

    // Set necessary state.
    //m_commandList->SetGraphicsRootSignature(m_boundPSO.rootSignature.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    bindDescriptorHeaps();

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_boundVertexBuffer.view);
    m_commandList->DrawInstanced(vertexCount, 1, 0, 0);

    m_boundVertexBuffer = {};
}

void EfgContext::DrawIndexedInstanced(uint32_t indexCount)
{
    bindDescriptorHeaps();
    m_commandList->IASetVertexBuffers(0, 1, &m_boundVertexBuffer.view);
    m_commandList->IASetIndexBuffer(&m_boundIndexBuffer.view);
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
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

void EfgContext::CreateBuffer(void const* data, EfgBuffer& buffer, EFG_CPU_ACCESS cpuAccess, D3D12_RESOURCE_STATES finalState)
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

void EfgContext::CreateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size)
{
    buffer.size = size;
    buffer.alignmentSize = (size + 255) & ~255;
    buffer.type = EFG_CONSTANT_BUFFER;
    CreateBuffer(data, buffer, EFG_CPU_WRITE, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    m_constantBuffers.push_back(&buffer);

    m_cbvDescriptorCount++;
}

void EfgContext::CreateStructuredBuffer(EfgStructuredBuffer& buffer, void const* data, UINT size, uint32_t count, size_t stride)
{
    buffer.size = size;
    buffer.alignmentSize = size;
    buffer.type = EFG_STRUCTURED_BUFFER;
    buffer.count = count;
    buffer.stride = stride;
    CreateBuffer(data, buffer, EFG_CPU_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    m_structuredBuffers.push_back(&buffer);

    m_srvDescriptorCount++;
}

void EfgContext::CreateTexture2D(EfgTexture& texture, const wchar_t* filename)
{
    ResourceUploadBatch resourceUpload(m_device.Get());
    resourceUpload.Begin();
    EFG_D3D_TRY(CreateWICTextureFromFile(m_device.Get(), resourceUpload, filename, texture.resource.ReleaseAndGetAddressOf()));
    auto uploadResourcesFinished = resourceUpload.End(m_commandQueue.Get());
    uploadResourcesFinished.wait();
    texture.index = m_textureCount;
    m_textures.push_back(&texture);
    m_textureCount++;
}

void EfgContext::CreateSampler(EfgSampler& sampler)
{
    sampler.desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.desc.MipLODBias = 0.0f;
    sampler.desc.MaxAnisotropy = 1;
    sampler.desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.desc.BorderColor[0] = 1.0f;
    sampler.desc.BorderColor[1] = 1.0f;
    sampler.desc.BorderColor[2] = 1.0f;
    sampler.desc.BorderColor[3] = 1.0f;
    sampler.desc.MinLOD = 0.0f;
    sampler.desc.MaxLOD = D3D12_FLOAT32_MAX;

    m_samplers.push_back(&sampler);
    
    m_samplerCount++;

}

void EfgContext::UpdateConstantBuffer(EfgConstantBuffer& buffer, void const* data, UINT size)
{
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    D3D12_RANGE writeRange = { 0, size };
    buffer.m_bufferResource->Map(0, &readRange, &mappedData);
    if (mappedData)
        memcpy(mappedData, data, (size_t)size);
    buffer.m_bufferResource->Unmap(0, &writeRange);
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

EfgProgram EfgContext::CreateProgram(LPCWSTR fileName)
{
    EfgProgram program = {};
    program.source = GetAssetFullPath(fileName);
    CompileProgram(program);
    return program;
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
    CommitShaderResources();

    // Describe and create the graphics pipeline state object (PSO).
    pso.desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    pso.desc.pRootSignature = rootSignature.Get().Get();
    pso.desc.VS = CD3DX12_SHADER_BYTECODE(program.vs.Get());
    pso.desc.PS = CD3DX12_SHADER_BYTECODE(program.ps.Get());
    pso.desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.desc.DepthStencilState.DepthEnable = FALSE;
    pso.desc.DepthStencilState.StencilEnable = FALSE;
    pso.desc.SampleMask = UINT_MAX;
    pso.desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.desc.NumRenderTargets = 1;
    pso.desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.desc.SampleDesc.Count = 1;
    EFG_D3D_TRY(m_device->CreateGraphicsPipelineState(&pso.desc, IID_PPV_ARGS(&pso.pipelineState)));
    return pso;
}

void EfgContext::SetPipelineState(EfgPSO pso)
{
    m_boundPSO = pso;
}

void EfgContext::BindVertexBuffer(EfgVertexBuffer buffer)
{
    m_boundVertexBuffer = buffer;
}

void EfgContext::BindIndexBuffer(EfgIndexBuffer buffer)
{
    m_boundIndexBuffer = buffer;
}

void EfgContext::Bind2DTexture(const EfgTexture& texture)
{
    m_boundTexture = &texture;
}

void EfgContext::CompileProgram(EfgProgram& program)
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    EFG_D3D_TRY(D3DCompileFromFile(program.source.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &program.vs, nullptr));
    EFG_D3D_TRY(D3DCompileFromFile(program.source.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &program.ps, nullptr));
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

D3D12_DESCRIPTOR_RANGE EfgDescriptorRange::Commit()
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
    descriptorRange.NumDescriptors = static_cast<UINT>(resources.size());
    descriptorRange.BaseShaderRegister = baseShaderRegister;
    descriptorRange.RegisterSpace = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    return descriptorRange;
}

D3D12_ROOT_PARAMETER EfgRootParameter::Commit()
{
    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
    rootParameter.DescriptorTable.pDescriptorRanges = ranges.data();
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
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
