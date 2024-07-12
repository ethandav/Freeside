#include "efg.h"
#include <filesystem>
#include <shlobj.h>

#include "Shapes.h"
#include "efg_camera.h"

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
    Camera camera = efgCreateCamera(efg, DirectX::XMFLOAT3(0.0f, 0.0f, -5.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

    Shape square = Shapes::getShape(Shapes::SPHERE);
    XMMATRIX transformMatrix = efgCreateTransformMatrix(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f));

    efgCreateCBVDescriptorHeap(efg, 3);

    EfgVertexBuffer vertexBuffer = efgCreateVertexBuffer<Vertex>(efg, square.vertices.data(), square.vertexCount);
    EfgIndexBuffer indexBuffer = efgCreateIndexBuffer<uint32_t>(efg, square.indices.data(), square.indexCount);
    EfgConstantBuffer viewProjBuffer = efgCreateConstantBuffer<XMMATRIX>(efg, &camera.viewProj, 1);
    EfgConstantBuffer transformBuffer = efgCreateConstantBuffer<XMMATRIX>(efg, &transformMatrix, 1);
    EfgConstantBuffer transformBuffer2 = efgCreateConstantBuffer<XMMATRIX>(efg, &transformMatrix, 1);

    EfgProgram program = efgCreateProgram(efg, L"shaders.hlsl");
    EfgPSO pso = efgCreateGraphicsPipelineState(efg, program);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgUpdateCamera(efg, efgWindow, camera);
        efgUpdateConstantBuffer(efg, viewProjBuffer, &camera.viewProj, sizeof(camera.viewProj));
        efgBindVertexBuffer(efg, vertexBuffer);
        efgBindIndexBuffer(efg, indexBuffer);
        efgSetPipelineState(efg, pso);
        efgDrawIndexedInstanced(efg, square.indexCount);
        efgRender(efg);
    }

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
