#include "efg.h"
#include <iostream>

EfgContext efgCreateContext(HWND window)
{
	EfgContext context = {};
	EfgInternal* efg = new EfgInternal(context);
	efg->initialize(window);
	return context;
}

void efgDestroyContext(EfgContext context)
{
	EfgInternal* efg = EfgInternal::GetEfg(context);
	delete efg;
}

void efgRender(EfgContext context)
{
	EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->Render();
}

void efgCreateCBVDescriptorHeap(EfgContext context, uint32_t numDescriptors)
{
	EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->createCBVDescriptorHeap(numDescriptors);
}

EfgBuffer efgCreateBuffer(EfgContext context, EFG_BUFFER_TYPE bufferType, void const* data, UINT size)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    return efg->CreateBuffer(bufferType, data, size);
}

void efgDestroyBuffer(EfgBuffer& buffer)
{
    switch (buffer.type)
    {
    case EFG_VERTEX_BUFFER:
        delete reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW*>(buffer.viewHandle);
        break;
    case EFG_INDEX_BUFFER:
        delete reinterpret_cast<D3D12_INDEX_BUFFER_VIEW*>(buffer.viewHandle);
        break;
    }
}

EfgProgram efgCreateProgram(EfgContext context, LPCWSTR fileName)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    return efg->CreateProgram(fileName);
}

EfgPSO efgCreateGraphicsPipelineState(EfgContext context, EfgProgram program)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    return efg->CreateGraphicsPipelineState(program);
}

void efgSetPipelineState(EfgContext context, EfgPSO pso)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->SetPipelineState(pso);
}

void efgBindVertexBuffer(EfgContext context, EfgBuffer buffer)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->bindVertexBuffer(buffer);
}

void efgBindIndexBuffer(EfgContext context, EfgBuffer buffer)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->bindIndexBuffer(buffer);
}

void efgDrawInstanced(EfgContext context, uint32_t vertexCount)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->DrawInstanced(vertexCount);
}

void efgDrawIndexedInstanced(EfgContext context, uint32_t indexCount)
{
    EfgInternal* efg = EfgInternal::GetEfg(context);
    efg->DrawIndexedInstanced(indexCount);
}

void EfgInternal::initialize(HWND window)
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

EfgInternal* EfgInternal::GetEfg(EfgContext& context)
{
	return reinterpret_cast<EfgInternal*>(context.handle);
}

// Helper function for resolving the full path of assets.
std::wstring EfgInternal::GetAssetFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void EfgInternal::GetHardwareAdapter(
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

// Load the rendering pipeline dependencies.
void EfgInternal::LoadPipeline()
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
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

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
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        window_,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    }

    {
        // Define a root parameter for the descriptor table
        D3D12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_DESCRIPTOR_RANGE descriptorRange;
        descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        descriptorRange.NumDescriptors = 1;
        descriptorRange.BaseShaderRegister = 0;
        descriptorRange.RegisterSpace = 0;
        descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;

        // Create the root signature description
        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                std::cerr << "D3D12SerializeRootSignature failed: " << static_cast<const char*>(errorBlob->GetBufferPointer()) << std::endl;
            }
            ThrowIfFailed(hr);
        }

        hr = m_device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                std::cerr << "CreateRootSignature failed: " << static_cast<const char*>(errorBlob->GetBufferPointer()) << std::endl;
            }
            ThrowIfFailed(hr);
        }
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void EfgInternal::LoadAssets()
{
    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

// Render the scene.
void EfgInternal::Render()
{
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(m_commandList->Close());

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void EfgInternal::createCBVDescriptorHeap(uint32_t numDescriptors)
{
    if (numDescriptors > 0)
    {
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = numDescriptors;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));

        m_cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
}

void EfgInternal::DrawInstanced(uint32_t vertexCount)
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_boundPSO.pipelineState.Get()));

    // Set necessary state.
    //m_commandList->SetGraphicsRootSignature(m_boundPSO.rootSignature.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // BInd descriptor heaps
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    // Set the root descriptor table.
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_boundVertexBuffer.viewHandle));
    m_commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void EfgInternal::DrawIndexedInstanced(uint32_t indexCount)
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_boundPSO.pipelineState.Get()));

    // Set necessary state.
    //m_commandList->SetGraphicsRootSignature(m_boundPSO.rootSignature.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // BInd descriptor heaps
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    // Set the root descriptor table.
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, reinterpret_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_boundVertexBuffer.viewHandle));
    m_commandList->IASetIndexBuffer(reinterpret_cast<D3D12_INDEX_BUFFER_VIEW*>(m_boundIndexBuffer.viewHandle));
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void EfgInternal::Destroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void EfgInternal::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

EfgBuffer EfgInternal::CreateBuffer(EFG_BUFFER_TYPE bufferType, void const* data, UINT size)
{
    EfgBuffer buffer = { };
    UINT alignmentSize = size;

    if(bufferType == EFG_CONSTANT_BUFFER)
        alignmentSize = (size + 255) & ~255; // CB size must be a multiple of 256 bytes.

    // Note: using upload heaps to transfer static data like vert buffers is not 
    // recommended. Every time the GPU needs it, the upload heap will be marshalled 
    // over. Please read up on Default Heap usage. An upload heap is used here for 
    // code simplicity and because there are very few verts to actually transfer.
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(alignmentSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer.m_bufferResource)));
    
    // Copy the triangle data to the vertex buffer.
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(buffer.m_bufferResource->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, data, size);
    buffer.m_bufferResource->Unmap(0, nullptr);
    buffer.m_size = size;
    buffer.type = bufferType;

    switch (bufferType)
    {
    case EFG_VERTEX_BUFFER:
        {
            D3D12_VERTEX_BUFFER_VIEW* view = new D3D12_VERTEX_BUFFER_VIEW;
            view->BufferLocation = buffer.m_bufferResource->GetGPUVirtualAddress();
            view->StrideInBytes = sizeof(Vertex);
            view->SizeInBytes = buffer.m_size;
            buffer.viewHandle = reinterpret_cast<uint64_t>(view); 
            break;
        }
    case EFG_INDEX_BUFFER:
        {
            D3D12_INDEX_BUFFER_VIEW* view = new D3D12_INDEX_BUFFER_VIEW;
            view->BufferLocation = buffer.m_bufferResource->GetGPUVirtualAddress();
            view->Format = DXGI_FORMAT_R32_UINT;
            view->SizeInBytes = buffer.m_size;
            buffer.viewHandle = reinterpret_cast<uint64_t>(view); 
            break;
        }
    case EFG_CONSTANT_BUFFER:
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
            cbvHandle.Offset(m_cbvDescriptorCount, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = buffer.m_bufferResource->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = alignmentSize; // CB size must be a multiple of 256 bytes.
            m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
            m_cbvDescriptorCount++;
            buffer.viewHandle = reinterpret_cast<uint64_t>(&cbvHandle);
            break;
        }
    };

    return buffer;
}

EfgProgram EfgInternal::CreateProgram(LPCWSTR fileName)
{
    EfgProgram program = {};
    program.source = GetAssetFullPath(fileName);
    return program;
}

EfgPSO EfgInternal::CreateGraphicsPipelineState(EfgProgram program)
{
    EfgPSO pso = {};
    CompileProgram(program);

    // Create an empty root signature.
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pso.rootSignature)));
    }

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO).
    pso.desc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    pso.desc.pRootSignature = m_rootSignature.Get();
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
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso.desc, IID_PPV_ARGS(&pso.pipelineState)));
    return pso;
}

void EfgInternal::SetPipelineState(EfgPSO pso)
{
    m_boundPSO = pso;
}

void EfgInternal::bindVertexBuffer(EfgBuffer buffer)
{
    m_boundVertexBuffer = buffer;
}

void EfgInternal::bindIndexBuffer(EfgBuffer buffer)
{

    m_boundIndexBuffer = buffer;
}

void EfgInternal::CompileProgram(EfgProgram& program)
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ThrowIfFailed(D3DCompileFromFile(program.source.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &program.vs, nullptr));
    ThrowIfFailed(D3DCompileFromFile(program.source.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &program.ps, nullptr));
}

