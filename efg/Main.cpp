#include "efg.h"
#include <filesystem>
#include <shlobj.h>

#include "Shapes.h"
#include "efg_camera.h"
#include <random>

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
    Camera camera = efgCreateCamera(efg, DirectX::XMFLOAT3(-18.0f, -14.0f, 6.7f), DirectX::XMFLOAT3(0.0f, -13.0f, 0.0f));

    Shape square = Shapes::getShape(Shapes::SPHERE);
	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
    std::vector<XMMATRIX> transformMatrices;
	transformMatrices.reserve(2000);
	for (int i = 0; i < 2000; i++)
	{
		transformMatrices.push_back(efgCreateTransformMatrix(XMFLOAT3(dist(rng), dist(rng), dist(rng)), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)));
	}
    XMMATRIX transformMatrix = efgCreateTransformMatrix(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f));
    transformMatrices.push_back(transformMatrix);
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
    std::vector<LightBuffer> lights(1);
    //lights[0].position = XMFLOAT4(30.0f, 10.0f, 0.0f, 0.0f);
    lights[0].position = XMFLOAT4(5.0f, 20.0f, 0.0f, 0.0f);
    lights[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

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

    EfgBuffer viewProjBuffer = efg.CreateConstantBuffer<XMMATRIX>(&camera.viewProj, 1);
    EfgBuffer transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&transformMatrix, 1);
    EfgBuffer viewPosBuffer = efg.CreateConstantBuffer<XMFLOAT3>(&camera.eye, 1);
    EfgBuffer materialBuffer = efg.CreateConstantBuffer<MaterialBuffer>(&material, 1);
    EfgBuffer lightDataBuffer = efg.CreateConstantBuffer<LightBuffer>(&lightData, 1);

    EfgBuffer lightBuffer = efg.CreateStructuredBuffer<LightBuffer>(lights.data(), (uint32_t)lights.size());
    EfgBuffer transformMatrixBuffer = efg.CreateStructuredBuffer<LightBuffer>(transformMatrices.data(), (uint32_t)transformMatrices.size());

    EfgTexture texture = efg.CreateTexture2D(L"earth.jpeg");
    EfgTexture texture2 = efg.CreateTexture2D(L"water.jpg");

    EfgSampler sampler = efg.CreateSampler();

    EfgImportMesh mesh = efg.LoadFromObj("C:\\Users\\Ethan\\Documents\\sibenik", "C:\\Users\\Ethan\\Documents\\sibenik\\sibenik.obj");
    //EfgImportMesh mesh = efg.LoadFromObj(nullptr, "C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\donut\\donut.obj");

    // Create a Skybox
    Shape skybox = Shapes::getShape(Shapes::SKYBOX);
    EfgVertexBuffer skyboxVertexBuffer = efg.CreateVertexBuffer<Vertex>(skybox.vertices.data(), skybox.vertexCount);
    EfgIndexBuffer skyboxIndexBuffer = efg.CreateIndexBuffer<uint32_t>(skybox.indices.data(), skybox.indexCount);
    std::wstring rightFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\right.png";
    std::wstring leftFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\left.png";
    std::wstring topFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\top.png";
    std::wstring bottomFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\bottom.png";
    std::wstring frontFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\front.png";
    std::wstring backFace = L"C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\skybox\\back.png";
    std::vector<std::wstring> skyboxTextures = {
        rightFace, leftFace, topFace, bottomFace, frontFace, backFace
    };
    EfgTexture skyBox = efg.CreateTextureCube(skyboxTextures);
    XMFLOAT4X4 skybox_view = camera.view;
    skybox_view._41 = 0.0f;
    skybox_view._42 = 0.0f;
    skybox_view._43 = 0.0f;
    skybox_view._44 = 1.0f;
    EfgBuffer skybox_viewBuffer = efg.CreateConstantBuffer<XMFLOAT4X4>(&skybox_view, 1);
    EfgBuffer skybox_projBuffer = efg.CreateConstantBuffer<XMFLOAT4X4>(&camera.proj, 1);

    // Must commit all resources before creating root signatures. This will pack all resources in the heap by type.
    efg.CommitShaderResources();

    // Skybox Root Signature
    EfgDescriptorRange skybox_range_CBV = EfgDescriptorRange(efgRange_CBV, 0);
    skybox_range_CBV.insert(skybox_viewBuffer);
    skybox_range_CBV.insert(skybox_projBuffer);
    EfgDescriptorRange skybox_range_cube = EfgDescriptorRange(efgRange_SRV, 0);
    skybox_range_cube.insert(skyBox);
    EfgDescriptorRange skybox_rangeSampler = EfgDescriptorRange(efgRange_SAMPLER, 0);
    skybox_rangeSampler.insert(sampler);
    EfgRootParameter skybox_rootParameter_1;
    EfgRootParameter skybox_rootParameter_2;
    EfgRootParameter skybox_rootParameter_3;
    skybox_rootParameter_1.insert(skybox_range_CBV);
    skybox_rootParameter_2.insert(skybox_range_cube);
    skybox_rootParameter_3.insert(skybox_rangeSampler);
    EfgRootSignature skybox_rootSignature;
    skybox_rootSignature.insert(skybox_rootParameter_1);
    skybox_rootSignature.insert(skybox_rootParameter_2);
    skybox_rootSignature.insert(skybox_rootParameter_3);
    efg.CreateRootSignature(skybox_rootSignature);
    EfgProgram skyboxProgram = efg.CreateProgram(L"skybox.hlsl");
    EfgPSO skyboxPso = efg.CreateGraphicsPipelineState(skyboxProgram, skybox_rootSignature);


    EfgDescriptorRange range = EfgDescriptorRange(efgRange_CBV, 0);
    range.insert(viewProjBuffer);
    range.insert(transformBuffer);
    range.insert(viewPosBuffer);
    range.insert(materialBuffer);
    range.insert(lightDataBuffer);

    EfgDescriptorRange materialRange = EfgDescriptorRange(efgRange_CBV, 5, 1);

    EfgDescriptorRange rangeSrv = EfgDescriptorRange(efgRange_SRV, 0);
    rangeSrv.insert(lightBuffer);
    rangeSrv.insert(transformMatrixBuffer);

    EfgDescriptorRange rangeTex = EfgDescriptorRange(efgRange_SRV, 2, 1);

    EfgDescriptorRange rangeSampler = EfgDescriptorRange(efgRange_SAMPLER, 0);
    rangeSampler.insert(sampler);

    EfgRootParameter rootParameter0;
    rootParameter0.insert(range);
    EfgRootParameter rootParameter1;
    rootParameter1.insert(materialRange);
    rootParameter1.data.conditionalBind = true;
    EfgRootParameter rootParameter2;
    rootParameter2.insert(rangeSrv);
    EfgRootParameter rootParameter3;
    rootParameter3.insert(rangeTex);
    rootParameter3.data.conditionalBind = true;

    EfgRootParameter rootParameter4;
    rootParameter4.insert(rangeSampler);

    EfgRootSignature rootSignature;
    rootSignature.insert(rootParameter0);
    rootSignature.insert(rootParameter1);
    rootSignature.insert(rootParameter2);
    rootSignature.insert(rootParameter3);
    rootSignature.insert(rootParameter4);
    efg.CreateRootSignature(rootSignature);

    EfgProgram program = efg.CreateProgram(L"shaders.hlsl");
    EfgPSO pso = efg.CreateGraphicsPipelineState(program, rootSignature);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgUpdateCamera(efg, efgWindow, camera);
        efg.Frame();
        efg.SetPipelineState(pso);
        efg.BindRootDescriptorTable(rootSignature);
        efg.UpdateConstantBuffer(viewProjBuffer, &camera.viewProj, sizeof(camera.viewProj));
        efg.UpdateConstantBuffer(viewPosBuffer, &camera.eye, sizeof(camera.eye));
        XMFLOAT4X4 skybox_view = camera.view;
        skybox_view._41 = 0.0f;
        skybox_view._42 = 0.0f;
        skybox_view._43 = 0.0f;
        skybox_view._44 = 1.0f;
        efg.UpdateConstantBuffer(skybox_viewBuffer, &skybox_view, sizeof(skybox_view));
        efg.UpdateConstantBuffer(skybox_projBuffer, &camera.proj, sizeof(camera.proj));
        efg.BindVertexBuffer(vertexBuffer);
        efg.BindIndexBuffer(indexBuffer);
        efg.Bind2DTexture(texture);
        efg.DrawIndexedInstanced(square.indexCount, 2000);

        for (size_t m = 0; m < mesh.materialBatches.size(); m++)
        {
            EfgInstanceBatch instances = mesh.materialBatches[m];
            if(mesh.textures[m].diffuse_map.handle > 0)
                efg.Bind2DTexture(mesh.textures[m].diffuse_map);
            efg.BindConstantBuffer(mesh.materialBuffers[m]);
            efg.BindVertexBuffer(instances.vertexBuffer);
            efg.BindIndexBuffer(instances.indexBuffer);
            efg.DrawIndexedInstanced(instances.indexCount);
        }

        efg.SetPipelineState(skyboxPso);
        efg.BindRootDescriptorTable(skybox_rootSignature);
        efg.BindVertexBuffer(skyboxVertexBuffer);
        efg.BindIndexBuffer(skyboxIndexBuffer);
        efg.DrawIndexedInstanced(skybox.indexCount);

        efg.Render();
    }

    efgDestroyWindow(efgWindow);

    return 0;
}
