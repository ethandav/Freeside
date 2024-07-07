#include "efg.h"
#include <filesystem>
#include <shlobj.h>

static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::filesystem::path pixInstallationPath = programFilesPath;
    pixInstallationPath /= "Microsoft PIX";

    std::wstring newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
    {
        if (directory_entry.is_directory())
        {
            if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
            {
                newestVersionFound = directory_entry.path().filename().c_str();
            }
        }
    }

    if (newestVersionFound.empty())
    {
        // TODO: Error, no PIX installation found
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

int main()
{
    if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
    {
        LoadLibrary(GetLatestWinPixGpuCapturerPath_Cpp17().c_str());
    }

    EfgWindow efgWindow = efgCreateWindow(1920, 1080, L"New Window");
    EfgContext efg = efgCreateContext(efgWindow);

    float aspectRatio = static_cast<float>(1920) / static_cast<float>(1080);

    // Define the geometry for a triangle.
    Vertex triangleVertices[] =
    {
        { { 0.0f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { 0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
    };

    // Define the geometry for a square
    Vertex squareVertices[] =
    {
        { { -0.5f,  0.5f * aspectRatio, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { {  0.5f,  0.5f * aspectRatio, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { {  0.5f, -0.5f * aspectRatio, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { { -0.5f, -0.5f * aspectRatio, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } }
    };

    uint32_t squareIndices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

    // View matrix
    DirectX::XMVECTOR eyePosition = DirectX::XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f);
    DirectX::XMVECTOR focusPoint = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR upDirection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    // Projection matrix
    float fieldOfView = DirectX::XMConvertToRadians(45.0f);
    float nearZ = 0.1f;
    float farZ = 100.0f;
    DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fieldOfView, aspectRatio, nearZ, farZ);

    // View-Projection matrix
    DirectX::XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    efgCreateCBVDescriptorHeap(efg, 2);

    int test = 12;

    //EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, triangleVertices, sizeof(triangleVertices), 3);
    EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, EFG_VERTEX_BUFFER, squareVertices, sizeof(squareVertices), 4);
    EfgBuffer indexBuffer = efgCreateBuffer<uint32_t>(efg, EFG_INDEX_BUFFER, squareIndices, sizeof(squareIndices), 6);
    EfgBuffer constantBuffer = efgCreateBuffer<XMMATRIX>(efg, EFG_CONSTANT_BUFFER, &viewProjectionMatrix, sizeof(viewProjectionMatrix), 1);
    EfgBuffer constantBuffer2 = efgCreateBuffer<int>(efg, EFG_CONSTANT_BUFFER, &test, sizeof(test), 1);

    EfgProgram program = efgCreateProgram(efg, L"shaders.hlsl");
    EfgPSO pso = efgCreateGraphicsPipelineState(efg, program);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgBindVertexBuffer(efg, vertexBuffer);
        efgBindIndexBuffer(efg, indexBuffer);
        efgSetPipelineState(efg, pso);
        efgDrawIndexedInstanced(efg, 6);
        efgRender(efg);
    }

    efgDestroyBuffer(vertexBuffer);
    efgDestroyBuffer(indexBuffer);

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
