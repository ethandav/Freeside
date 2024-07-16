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
    EfgContext efg;
    efg.initialize(efgWindow);
    Camera camera = efgCreateCamera(efg, DirectX::XMFLOAT3(0.0f, 0.0f, -5.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

    Shape square = Shapes::getShape(Shapes::SPHERE);
    XMMATRIX transformMatrix = efgCreateTransformMatrix(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f));
    XMMATRIX transformMatrix2 = efgCreateTransformMatrix(XMFLOAT3(2.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f));

    struct LightBuffer
    {
        XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        XMFLOAT4 position = XMFLOAT4(0.0f, 0.0f, -2.0f, 0.0f);
        XMFLOAT4 ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        XMFLOAT4 diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        XMFLOAT4 specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
        XMFLOAT4 attenuation = XMFLOAT4(1.0f, 0.009f, 0.0032f, 0.0f);
    };
    std::vector<LightBuffer> lights(2);
    lights[0].position = XMFLOAT4(30.0f, 10.0f, 0.0f, 0.0f);
    lights[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

    lights[1].position = XMFLOAT4(-30.0f, 10.0f, 0.0f, 0.0f);
    lights[1].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

    struct LightConstants
    {
        uint32_t numPointLights = 0;
    };
    LightConstants lightData;
    lightData.numPointLights = (uint32_t)lights.size();

    struct MaterialBuffer
    {
        XMFLOAT4 ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
        XMFLOAT4 diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
        XMFLOAT4 specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
        float shininess = 32.0f;
        float padding[3];
    };
    MaterialBuffer material;

    EfgVertexBuffer vertexBuffer = efg.CreateVertexBuffer<Vertex>(square.vertices.data(), square.vertexCount);
    EfgIndexBuffer indexBuffer = efg.CreateIndexBuffer<uint32_t>(square.indices.data(), square.indexCount);

    EfgConstantBuffer
        viewProjBuffer,
        transformBuffer,
        viewPosBuffer,
        materialBuffer,
        lightDataBuffer;

    efg.CreateConstantBuffer<XMMATRIX>(viewProjBuffer, &camera.viewProj, 1);
    efg.CreateConstantBuffer<XMMATRIX>(transformBuffer, &transformMatrix, 1);
    efg.CreateConstantBuffer<XMFLOAT3>(viewPosBuffer, &camera.eye, 1);
    efg.CreateConstantBuffer<MaterialBuffer>(materialBuffer, &material, 1);
    efg.CreateConstantBuffer<LightBuffer>(lightDataBuffer, &lightData, 1);

    EfgStructuredBuffer lightBuffer;
    efg.CreateStructuredBuffer<LightBuffer>(lightBuffer, lights.data(), (uint32_t)lights.size());

    EfgTexture texture;
    efg.CreateTexture2D(texture, L"earth.jpeg");
    EfgTexture texture2;
    efg.CreateTexture2D(texture2, L"water.jpg");

    EfgSampler sampler;
    efg.CreateSampler(sampler);

    EfgDescriptorRange range = EfgDescriptorRange(efgRange_CBV, 0);
    range.insert(viewProjBuffer);
    range.insert(transformBuffer);
    range.insert(viewPosBuffer);
    range.insert(materialBuffer);
    range.insert(lightDataBuffer);

    EfgDescriptorRange rangeSrv = EfgDescriptorRange(efgRange_SRV, 0);
    rangeSrv.insert(lightBuffer);

    EfgDescriptorRange rangeTex = EfgDescriptorRange(efgRange_SRV, 1, 1);

    EfgDescriptorRange rangeSampler = EfgDescriptorRange(efgRange_SAMPLER, 0);
    rangeSampler.insert(sampler);

    EfgRootParameter rootParameter0;
    rootParameter0.insert(range);
    EfgRootParameter rootParameter1;
    rootParameter1.insert(rangeSrv);
    EfgRootParameter rootParameter2;
    rootParameter2.insert(rangeTex);
    rootParameter2.data.conditionalBind = true;

    EfgRootParameter rootParameter3;
    rootParameter3.insert(rangeSampler);

    EfgRootSignature rootSignature;
    rootSignature.insert(rootParameter0);
    rootSignature.insert(rootParameter1);
    rootSignature.insert(rootParameter2);
    rootSignature.insert(rootParameter3);
    efg.CreateRootSignature(rootSignature);

    EfgProgram program = efg.CreateProgram(L"shaders.hlsl");
    EfgPSO pso = efg.CreateGraphicsPipelineState(program, rootSignature);
    efg.SetPipelineState(pso);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgUpdateCamera(efg, efgWindow, camera);
        efg.Frame();
        efg.BindRootDescriptorTable(rootSignature);
        efg.UpdateConstantBuffer(viewProjBuffer, &camera.viewProj, sizeof(camera.viewProj));
        efg.UpdateConstantBuffer(viewPosBuffer, &camera.eye, sizeof(camera.eye));
        efg.BindVertexBuffer(vertexBuffer);
        efg.BindIndexBuffer(indexBuffer);
        efg.Bind2DTexture(texture);
        efg.DrawIndexedInstanced(square.indexCount);
        efg.Bind2DTexture(texture2);
        efg.DrawIndexedInstanced(square.indexCount);

        efg.Render();
    }

    efgDestroyWindow(efgWindow);

    return 0;
}
