#include "efg.h"
#include <filesystem>
#include <shlobj.h>

#include "Shapes.h"
#include "efg_camera.h"
#include <random>

#include "efg_gameObject.h"
#include <iostream>

static LARGE_INTEGER frequency;
static LARGE_INTEGER startTime;
static bool isTimerInitialized = false;

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

void InitializeTimer()
{
    if (!isTimerInitialized)
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startTime);
        isTimerInitialized = true;
    }
}

double GetTimeInSeconds()
{
    if (!isTimerInitialized)
        InitializeTimer();

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);

    LONGLONG elapsedTicks = currentTime.QuadPart - startTime.QuadPart;

    return static_cast<double>(elapsedTicks) / static_cast<double>(frequency.QuadPart);
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
    Camera camera = efgCreateCamera(efg, DirectX::XMFLOAT3(0.0f, 0.0f, 5.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

    Shape square = Shapes::getShape(Shapes::SPHERE);
	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
    std::vector<XMMATRIX> transformMatrices;
	transformMatrices.reserve(2000);
	for (int i = 0; i < 2000; i++)
	{
		transformMatrices.push_back(efgCreateTransformMatrix(XMFLOAT3(dist(rng), dist(rng), dist(rng)), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)));
	}
    InstanceableObject sphereInstanced;
    sphereInstanced.vertexBuffer = efg.CreateVertexBuffer<Vertex>(square.vertices.data(), square.vertexCount);
    sphereInstanced.indexBuffer = efg.CreateIndexBuffer<uint32_t>(square.indices.data(), square.indexCount);
    sphereInstanced.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&sphereInstanced.constants, 1);

    GameObject sphere;
    sphere.vertexBuffer = efg.CreateVertexBuffer<Vertex>(square.vertices.data(), square.vertexCount);
    sphere.indexBuffer = efg.CreateIndexBuffer<uint32_t>(square.indices.data(), square.indexCount);
    sphere.material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    sphere.material.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    sphere.material.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    sphere.material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    sphere.material.shininess = 32.0f;
    sphere.material.diffuseMapFlag = true;
    EfgBuffer materialBuffer = efg.CreateConstantBuffer<EfgMaterialBuffer>(&sphere.material, 1);
    sphere.transform.translation = XMFLOAT3(1.0f, 1.0f, 1.0f);
    sphere.transform.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    sphere.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    sphere.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&sphere.constants, 1);
    sphere.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&sphere.transform.GetTransformMatrix(), 1);

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
    lights[0].position = XMFLOAT4(0.0f, 5.0f, 5.0f, 0.0f);
    lights[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);

    struct LightConstants
    {
        uint32_t numPointLights = 0;
    };
    LightConstants lightData;
    lightData.numPointLights = (uint32_t)lights.size();


    EfgBuffer viewProjBuffer = efg.CreateConstantBuffer<XMMATRIX>(&camera.viewProj, 1);
    EfgBuffer viewPosBuffer = efg.CreateConstantBuffer<XMFLOAT3>(&camera.eye, 1);
    EfgBuffer lightDataBuffer = efg.CreateConstantBuffer<LightBuffer>(&lightData, 1);

    EfgBuffer lightBuffer = efg.CreateStructuredBuffer<LightBuffer>(lights.data(), (uint32_t)lights.size());
    EfgBuffer transformMatrixBuffer = efg.CreateStructuredBuffer<LightBuffer>(transformMatrices.data(), (uint32_t)transformMatrices.size());

    EfgTexture texture = efg.CreateTexture2D(L"earth.jpeg");
    EfgTexture texture2 = efg.CreateTexture2D(L"water.jpg");

    EfgSampler sampler = efg.CreateSampler();

    //EfgImportMesh mesh = efg.LoadFromObj("C:\\Users\\Ethan\\Documents\\sibenik", "C:\\Users\\Ethan\\Documents\\sibenik\\sibenik.obj");
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
    EfgRootParameter skybox_rootParameter_1(efgRootParamter_DESCRIPTOR_TABLE);
    EfgRootParameter skybox_rootParameter_2(efgRootParamter_DESCRIPTOR_TABLE);
    EfgRootParameter skybox_rootParameter_3(efgRootParamter_DESCRIPTOR_TABLE);
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
    range.insert(viewPosBuffer);
    range.insert(lightDataBuffer);

    EfgDescriptorRange rangeSrv = EfgDescriptorRange(efgRange_SRV, 0);
    rangeSrv.insert(lightBuffer);
    rangeSrv.insert(transformMatrixBuffer);

    EfgDescriptorRange rangeTex = EfgDescriptorRange(efgRange_SRV, 2, 1);

    EfgDescriptorRange rangeSampler = EfgDescriptorRange(efgRange_SAMPLER, 0);
    rangeSampler.insert(sampler);

    EfgRootParameter rootParameter0(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter0.insert(range);
    EfgRootParameter rootParameter1(efgRootParamter_CBV); // Transform
    EfgRootParameter rootParameter2(efgRootParamter_CBV); // Object constants
    EfgRootParameter rootParameter3(efgRootParamter_CBV); // Material
    EfgRootParameter rootParameter4(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter4.insert(rangeSrv);
    EfgRootParameter rootParameter5(efgRootParamter_DESCRIPTOR_TABLE); // Texture
    rootParameter5.insert(rangeTex);

    EfgRootParameter rootParameter6(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter6.insert(rangeSampler);

    EfgRootSignature rootSignature;
    rootSignature.insert(rootParameter0);
    rootSignature.insert(rootParameter1);
    rootSignature.insert(rootParameter2);
    rootSignature.insert(rootParameter3);
    rootSignature.insert(rootParameter4);
    rootSignature.insert(rootParameter5);
    rootSignature.insert(rootParameter6);
    efg.CreateRootSignature(rootSignature);

    EfgProgram program = efg.CreateProgram(L"shaders.hlsl");
    EfgPSO pso = efg.CreateGraphicsPipelineState(program, rootSignature);

    float deltaTime = 0.0f;
    float lastFrameTime = GetTimeInSeconds();

    while (efgWindowIsRunning(efgWindow))
    {
        float currentFrameTime = GetTimeInSeconds();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

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

        efg.BindVertexBuffer(sphere.vertexBuffer);
        efg.BindIndexBuffer(sphere.indexBuffer);
        efg.Bind2DTexture(texture);
        efg.BindConstantBuffer(1, sphere.transformBuffer);
        efg.BindConstantBuffer(2, sphere.constantsBuffer);
        efg.BindConstantBuffer(3, materialBuffer);
        efg.DrawIndexedInstanced(square.indexCount, 1);

        efg.BindConstantBuffer(2, sphereInstanced.constantsBuffer);
        efg.DrawIndexedInstanced(square.indexCount, 2000);

        //for (size_t m = 0; m < mesh.materialBatches.size(); m++)
        //{
        //    EfgInstanceBatch instances = mesh.materialBatches[m];
        //    if(mesh.textures[m].diffuse_map.handle > 0)
        //        efg.Bind2DTexture(mesh.textures[m].diffuse_map);
        //    efg.BindConstantBuffer(mesh.materialBuffers[m]);
        //    efg.BindVertexBuffer(instances.vertexBuffer);
        //    efg.BindIndexBuffer(instances.indexBuffer);
        //    efg.DrawIndexedInstanced(instances.indexCount);
        //}

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

