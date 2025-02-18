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
    Camera camera = efgCreateCamera(efg, DirectX::XMFLOAT3(0.0f, 5.0f, -5.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

    RECT windowRect = {};
    GetClientRect(efgWindow, &windowRect);
    uint32_t windowWidth = windowRect.right - windowRect.left;
    uint32_t windowHeight = windowRect.bottom - windowRect.top;

    efg.OpenCommandList();

    EfgTexture depthBuffer = efg.CreateDepthBuffer(windowWidth,windowHeight);
    EfgTexture colorBuffer = efg.CreateColorBuffer(windowWidth,windowHeight);
    EfgTexture shadowMap = efg.CreateShadowMap(2048,2048);
    EfgTexture cubeShadowMap = efg.CreateCubeShadowMap(2048,2048);

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
    sphereInstanced.constants.useTransform = true;
    sphereInstanced.vertexBuffer = efg.CreateVertexBuffer<Vertex>(square.vertices.data(), square.vertexCount);
    sphereInstanced.indexBuffer = efg.CreateIndexBuffer<uint32_t>(square.indices.data(), square.indexCount);
    sphereInstanced.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&sphereInstanced.constants, 1);

    GameObject sphere;
    sphere.constants.useTransform = true;
    sphere.vertexBuffer = efg.CreateVertexBuffer<Vertex>(square.vertices.data(), square.vertexCount);
    sphere.indexBuffer = efg.CreateIndexBuffer<uint32_t>(square.indices.data(), square.indexCount);
    sphere.material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    sphere.material.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    sphere.material.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    sphere.material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    sphere.material.shininess = 32.0f;
    sphere.material.diffuseMapFlag = false;
    EfgBuffer materialBuffer = efg.CreateConstantBuffer<EfgMaterialBuffer>(&sphere.material, 1);
    sphere.transform.translation = XMFLOAT3(1.5f, 0.5f, 1.0f);
    sphere.transform.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    sphere.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    sphere.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&sphere.constants, 1);
    sphere.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&sphere.transform.GetTransformMatrix(), 1);

    GameObject cube;
    Shape cubeShape = Shapes::getShape(Shapes::CUBE);
    cube.constants.useTransform = true;
    cube.vertexBuffer = efg.CreateVertexBuffer<Vertex>(cubeShape.vertices.data(), cubeShape.vertexCount);
    cube.indexBuffer = efg.CreateIndexBuffer<uint32_t>(cubeShape.indices.data(), cubeShape.indexCount);
    cube.material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    cube.material.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    cube.material.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    cube.material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    cube.material.shininess = 32.0f;
    cube.material.diffuseMapFlag = false;
    EfgBuffer cubeMaterialBuffer = efg.CreateConstantBuffer<EfgMaterialBuffer>(&cube.material, 1);
    cube.transform.translation = XMFLOAT3(-1.5f, 0.5f, 1.0f);
    cube.transform.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    cube.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    cube.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&cube.constants, 1);
    cube.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&cube.transform.GetTransformMatrix(), 1);

    GameObject cube2;
    cube2.constants.useTransform = true;
    cube2.vertexBuffer = efg.CreateVertexBuffer<Vertex>(cubeShape.vertices.data(), cubeShape.vertexCount);
    cube2.indexBuffer = efg.CreateIndexBuffer<uint32_t>(cubeShape.indices.data(), cubeShape.indexCount);
    cube2.transform.translation = XMFLOAT3(0.0f, 2.5f, 7.0f);
    cube2.transform.scale = XMFLOAT3(7.0f, 7.0f, 7.0f);
    cube2.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    cube2.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&cube2.constants, 1);
    cube2.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&cube2.transform.GetTransformMatrix(), 1);
    GameObject cube3;
    cube3.constants.useTransform = true;
    cube3.vertexBuffer = efg.CreateVertexBuffer<Vertex>(cubeShape.vertices.data(), cubeShape.vertexCount);
    cube3.indexBuffer = efg.CreateIndexBuffer<uint32_t>(cubeShape.indices.data(), cubeShape.indexCount);
    cube3.transform.translation = XMFLOAT3(7.0f, 2.5f, 0.0f);
    cube3.transform.scale = XMFLOAT3(7.0f, 7.0f, 7.0f);
    cube3.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    cube3.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&cube3.constants, 1);
    cube3.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&cube3.transform.GetTransformMatrix(), 1);
    GameObject cube4;
    cube4.constants.useTransform = true;
    cube4.vertexBuffer = efg.CreateVertexBuffer<Vertex>(cubeShape.vertices.data(), cubeShape.vertexCount);
    cube4.indexBuffer = efg.CreateIndexBuffer<uint32_t>(cubeShape.indices.data(), cubeShape.indexCount);
    cube4.transform.translation = XMFLOAT3(-7.0f, 2.5f, 0.0f);
    cube4.transform.scale = XMFLOAT3(7.0f, 7.0f, 7.0f);
    cube4.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    cube4.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&cube4.constants, 1);
    cube4.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&cube4.transform.GetTransformMatrix(), 1);

    GameObject plane;
    Shape planeShape = Shapes::getShape(Shapes::PLANE);
    plane.constants.useTransform = true;
    plane.vertexBuffer = efg.CreateVertexBuffer<Vertex>(planeShape.vertices.data(), planeShape.vertexCount);
    plane.indexBuffer = efg.CreateIndexBuffer<uint32_t>(planeShape.indices.data(), planeShape.indexCount);
    plane.material.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    plane.material.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    plane.material.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);
    plane.material.specular = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.0f);
    plane.material.shininess = 32.0f;
    plane.material.diffuseMapFlag = false;
    EfgBuffer planeMaterialBuffer = efg.CreateConstantBuffer<EfgMaterialBuffer>(&plane.material, 1);
    plane.transform.translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    plane.transform.scale = XMFLOAT3(1.5f, 1.5f, 1.5f);
    plane.transform.rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    plane.constantsBuffer = efg.CreateConstantBuffer<ObjectConstants>(&plane.constants, 1);
    plane.transformBuffer = efg.CreateConstantBuffer<XMMATRIX>(&plane.transform.GetTransformMatrix(), 1);

    std::vector<PointLightBuffer> pointLights(1);
    pointLights[0].position = XMFLOAT4(0.0f, 0.5f, 0.0f, 0.0f);
    pointLights[0].color = XMFLOAT4(1.0f, 0.1f, 0.7f, 0.0f);
    pointLights[0].attenuation = XMFLOAT4(1.0, 0.09, 0.032, 0.0f);
    XMVECTOR lightPos = XMLoadFloat4(&pointLights[0].position);
    DirectX::XMMATRIX PL_shadowViewMatrices[6] = {
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),  // +X good
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // -X good
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)),  // +Y good
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), // -Y good
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),  // +Z good
        DirectX::XMMatrixLookAtLH(lightPos, lightPos + DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))  // -Z good
    };
    float nearPlane = 0.1f;
    float farPlane = 10.0f;
    XMMATRIX PL_projMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, nearPlane, farPlane);
    std::vector<EfgBuffer> pl_viewProjBuffers = {};
    pl_viewProjBuffers.reserve(6);
    for(int i = 0; i < 6; i++)
        pl_viewProjBuffers.push_back(efg.CreateConstantBuffer<XMMATRIX>(&(XMMatrixMultiply(PL_shadowViewMatrices[i], PL_projMatrix)), 1));


    DirLightBuffer dirLight;
    dirLight.direction = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    dirLight.ambient = XMFLOAT4(0.05f, 0.05f, 0.05f, 0.0f);
    dirLight.diffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 0.0f);
    dirLight.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.0f);

    // Position and look-at for the light
    DirectX::XMVECTOR lightPosition = DirectX::XMVectorSet(20.0f, 10.0f, 0.0f, 1.0f); // Position above the plane
    DirectX::XMVECTOR lightTarget = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // Center of the scene
    DirectX::XMVECTOR lightUp = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);    // Up direction for downward view
    
    // Calculate the direction vector from position to target
    DirectX::XMVECTOR lightDirectionVector = DirectX::XMVectorSubtract(lightTarget, lightPosition);
    lightDirectionVector = DirectX::XMVector3Normalize(lightDirectionVector);
    
    // Store the direction in dirLight (assuming it�s in float4 format)
    DirectX::XMStoreFloat4(&dirLight.direction, lightDirectionVector);

    DirectX::XMMATRIX lightViewMatrix = DirectX::XMMatrixLookAtLH(lightPosition, lightTarget, lightUp);
    
    // Ortho projection to cover the scene dimensions
    float orthoWidth = 20.0f; // Match scene size in X
    float orthoHeight = 20.0f; // Match scene size in Z
    float nearZ = 15.0f;
    float farZ = 35.0f;
    DirectX::XMMATRIX lightProjMatrix = DirectX::XMMatrixOrthographicLH(orthoWidth, orthoHeight, nearZ, farZ);
    
    // Combine view and projection for shadow mapping
    DirectX::XMMATRIX lightViewProjMatrix = lightViewMatrix * lightProjMatrix;

    LightConstants lightData;
    lightData.numPointLights = (uint32_t)pointLights.size(); 

    EfgBuffer viewProjBuffer = efg.CreateConstantBuffer<XMMATRIX>(&camera.viewProj, 1);
    EfgBuffer viewPosBuffer = efg.CreateConstantBuffer<XMFLOAT3>(&camera.eye, 1);
    EfgBuffer lightDataBuffer = efg.CreateConstantBuffer<PointLightBuffer>(&lightData, 1);
    EfgBuffer dirLightViewProj = efg.CreateConstantBuffer<XMMATRIX>(&lightViewProjMatrix, 1);
    //EfgBuffer dirLightViewProj = efg.CreateConstantBuffer<XMMATRIX>(&testMat, 1);
    EfgBuffer pointLightBuffer = efg.CreateStructuredBuffer<PointLightBuffer>(pointLights.data(), (uint32_t)pointLights.size());
    EfgBuffer dirLightBuffer = efg.CreateConstantBuffer<DirLightBuffer>(&dirLight, 1);
    EfgBuffer transformMatrixBuffer = efg.CreateStructuredBuffer<XMMATRIX>(transformMatrices.data(), (uint32_t)transformMatrices.size());

    //EfgTexture texture = efg.CreateTexture2DFromFile(L"earth.jpeg");
    //EfgTexture texture2 = efg.CreateTexture2DFromFile(L"grass.png");
    //EfgTexture textureBox = efg.CreateTexture2DFromFile(L"box.jpg");

    EfgSampler sampler = efg.CreateTextureSampler();
    EfgSampler depthSampler = efg.CreateDepthSampler();
    EfgSampler depthCubeSampler = efg.CreateDepthCubeSampler();

    //EfgImportMesh mesh = efg.LoadFromObj("C:\\Users\\Ethan\\Documents\\sibenik", "C:\\Users\\Ethan\\Documents\\sibenik\\sibenik.obj");
    //EfgImportMesh mesh = efg.LoadFromObj(nullptr, "C:\\Users\\Ethan\\Documents\\FreesideEngineTestAssets\\donut\\donut.obj");

    // Create a Skybox
    Shape skybox = Shapes::getShape(Shapes::SKYBOX);
    EfgBuffer skyboxVertexBuffer = efg.CreateVertexBuffer<Vertex>(skybox.vertices.data(), skybox.vertexCount);
    EfgBuffer skyboxIndexBuffer = efg.CreateIndexBuffer<uint32_t>(skybox.indices.data(), skybox.indexCount);
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
    EfgProgram skyBox_program;
    skyBox_program.vertexShader = efg.CreateShader(L"skybox.hlsl", "vs_5_0", "VSMain");
    skyBox_program.pixelShader = efg.CreateShader(L"skybox.hlsl", "ps_5_0", "PSMain");
    EfgPSO skyboxPso = efg.CreateGraphicsPipelineState(skyBox_program, skybox_rootSignature);

    EfgDescriptorRange range = EfgDescriptorRange(efgRange_CBV, 0);
    range.insert(viewProjBuffer);
    range.insert(viewPosBuffer);
    range.insert(lightDataBuffer);
    range.insert(dirLightViewProj);
    EfgDescriptorRange rangeSrv = EfgDescriptorRange(efgRange_SRV, 0);
    rangeSrv.insert(pointLightBuffer);
    rangeSrv.insert(transformMatrixBuffer);
    EfgDescriptorRange rangeTex = EfgDescriptorRange(efgRange_SRV, 2, 1);
    EfgDescriptorRange rangeShadowMap = EfgDescriptorRange(efgRange_SRV, 3, 1);
    EfgDescriptorRange rangeShadowCubeMap = EfgDescriptorRange(efgRange_SRV, 4, 1);
    EfgDescriptorRange rangeSampler = EfgDescriptorRange(efgRange_SAMPLER, 0);
    rangeSampler.insert(sampler);
    rangeSampler.insert(depthSampler);
    rangeSampler.insert(depthCubeSampler);
    EfgRootParameter rootParameter0(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter0.insert(range);
    EfgRootParameter rootParameter1(efgRootParameter_CBV); // Transform
    EfgRootParameter rootParameter2(efgRootParameter_CBV); // Object constants
    EfgRootParameter rootParameter3(efgRootParameter_CBV); // Material
    EfgRootParameter dirLightRootParameter(efgRootParameter_CBV); //Dir Light
    EfgRootParameter rootParameter4(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter4.insert(rangeSrv);
    EfgRootParameter rootParameter5(efgRootParamter_DESCRIPTOR_TABLE); // Texture
    rootParameter5.insert(rangeTex);
    EfgRootParameter rootParameter6(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter6.insert(rangeSampler);
    EfgRootParameter rootParameter7(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter7.insert(rangeShadowMap);
    EfgRootParameter rootParameter8(efgRootParamter_DESCRIPTOR_TABLE);
    rootParameter8.insert(rangeShadowCubeMap);
    EfgRootSignature rootSignature;
    rootSignature.insert(rootParameter0);
    rootSignature.insert(rootParameter1);
    rootSignature.insert(rootParameter2);
    rootSignature.insert(rootParameter3);
    rootSignature.insert(dirLightRootParameter);
    rootSignature.insert(rootParameter4);
    rootSignature.insert(rootParameter5);
    rootSignature.insert(rootParameter6);
    rootSignature.insert(rootParameter7);
    rootSignature.insert(rootParameter8);
    efg.CreateRootSignature(rootSignature);

    EfgRootParameter shadowMap_rootParameter0(efgRootParameter_CBV);
    EfgRootParameter shadowMap_rootParameter1(efgRootParameter_CBV);
    EfgRootParameter shadowMap_rootParameter2(efgRootParameter_CBV);
    EfgRootParameter shadowMap_rootParameter3(efgRootParamter_SRV);

    EfgRootSignature shadowMap_rootSignature;
    shadowMap_rootSignature.insert(shadowMap_rootParameter0);
    shadowMap_rootSignature.insert(shadowMap_rootParameter1);
    shadowMap_rootSignature.insert(shadowMap_rootParameter2);
    shadowMap_rootSignature.insert(shadowMap_rootParameter3);
    efg.CreateRootSignature(shadowMap_rootSignature);

    EfgProgram program;
    program.vertexShader = efg.CreateShader(L"vertex.hlsl", "vs_5_0");
    program.pixelShader = efg.CreateShader(L"shaders.hlsl", "ps_5_0");
    EfgPSO pso = efg.CreateGraphicsPipelineState(program, rootSignature);

    EfgProgram shadowMap_program;
    shadowMap_program.vertexShader = efg.CreateShader(L"shadowMap_vertex.hlsl", "vs_5_0");
    EfgPSO shadowMapPSO = efg.CreateShadowMapPSO(shadowMap_program, shadowMap_rootSignature);

    double deltaTime = 0.0f;
    double lastFrameTime = GetTimeInSeconds();

    efg.ExecuteCommandList();
    efg.WaitForGpu();

    while (efgWindowIsRunning(efgWindow))
    {
        double currentFrameTime = GetTimeInSeconds();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        efgWindowPumpEvents(efgWindow);
        efgUpdateCamera(efg, efgWindow, camera);
        efg.Frame();
        efg.UpdateConstantBuffer(viewProjBuffer, &camera.viewProj, sizeof(camera.viewProj));
        efg.UpdateConstantBuffer(viewPosBuffer, &camera.eye, sizeof(camera.eye));

        XMFLOAT4X4 skybox_view = camera.view;
        skybox_view._41 = 0.0f;
        skybox_view._42 = 0.0f;
        skybox_view._43 = 0.0f;
        skybox_view._44 = 1.0f;
        efg.UpdateConstantBuffer(skybox_viewBuffer, &skybox_view, sizeof(skybox_view));
        efg.UpdateConstantBuffer(skybox_projBuffer, &camera.proj, sizeof(camera.proj));

        // Shadow Maps
        {
            // Dir Light Shadow map
            {
                efg.SetPipelineState(shadowMapPSO);
                efg.BindRootDescriptorTable(shadowMap_rootSignature);
                efg.ClearDepthStencilView(shadowMap);
                efg.SetRenderTarget(shadowMap);
                efg.SetRenderTargetResolution(2048, 2048);
                efg.BindConstantBuffer(0, dirLightViewProj);
                efg.BindStructuredBuffer(3, transformMatrixBuffer);

                efg.BindVertexBuffer(sphere.vertexBuffer);
                efg.BindIndexBuffer(sphere.indexBuffer);
                efg.BindConstantBuffer(1, sphere.transformBuffer);
                efg.BindConstantBuffer(2, sphere.constantsBuffer);
                efg.DrawIndexedInstanced(square.indexCount, 1);

                efg.BindVertexBuffer(cube.vertexBuffer);
                efg.BindIndexBuffer(cube.indexBuffer);
                efg.BindConstantBuffer(1, cube.transformBuffer);
                efg.BindConstantBuffer(2, cube.constantsBuffer);
                efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

                efg.BindVertexBuffer(plane.vertexBuffer);
                efg.BindIndexBuffer(plane.indexBuffer);
                efg.BindConstantBuffer(1, plane.transformBuffer);
                efg.BindConstantBuffer(2, plane.constantsBuffer);
                efg.DrawIndexedInstanced(planeShape.indexCount, 1);

                //efg.BindConstantBuffer(2, sphereInstanced.constantsBuffer);
                //efg.DrawIndexedInstanced(square.indexCount, 2000);

                //for (size_t m = 0; m < mesh.materialBatches.size(); m++)
                //{
                //    EfgInstanceBatch instances = mesh.materialBatches[m];
                //    efg.BindConstantBuffer(2, mesh.constantsBuffer);
                //    efg.BindVertexBuffer(instances.vertexBuffer);
                //    efg.BindIndexBuffer(instances.indexBuffer);
                //    efg.DrawIndexedInstanced(instances.indexCount);
                //}
            }

            // Point Light Shadow map
            {
                efg.ClearDepthStencilView(cubeShadowMap);
                efg.SetRenderTargetResolution(2048, 2048);
                for (int i = 0; i < 6; i++)
                {
                    efg.SetRenderTarget(cubeShadowMap, i);
                    efg.BindConstantBuffer(0, pl_viewProjBuffers[i]);

                    efg.BindVertexBuffer(sphere.vertexBuffer);
                    efg.BindIndexBuffer(sphere.indexBuffer);
                    efg.BindConstantBuffer(1, sphere.transformBuffer);
                    efg.BindConstantBuffer(2, sphere.constantsBuffer);
                    efg.DrawIndexedInstanced(square.indexCount, 1);

                    efg.BindVertexBuffer(cube.vertexBuffer);
                    efg.BindIndexBuffer(cube.indexBuffer);
                    efg.BindConstantBuffer(1, cube.transformBuffer);
                    efg.BindConstantBuffer(2, cube.constantsBuffer);
                    efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

                    //efg.BindConstantBuffer(2, sphereInstanced.constantsBuffer);
                    //efg.DrawIndexedInstanced(square.indexCount, 2000);
                }
            }
        }

        // Main color render pass
        {
            efg.SetPipelineState(pso);
            efg.BindRootDescriptorTable(rootSignature);
            efg.BindConstantBuffer(4, dirLightBuffer);
            efg.Bind2DTexture(8, shadowMap);
            efg.Bind2DTexture(9, cubeShadowMap);

            efg.SetRenderTarget(colorBuffer, 0, &depthBuffer);
            efg.SetRenderTargetResolution(1920, 1080);
            efg.ClearRenderTargetView(colorBuffer);
            efg.ClearDepthStencilView(depthBuffer);

            efg.BindVertexBuffer(sphere.vertexBuffer);
            efg.BindIndexBuffer(sphere.indexBuffer);
            //efg.Bind2DTexture(6, texture);
            efg.BindConstantBuffer(1, sphere.transformBuffer);
            efg.BindConstantBuffer(2, sphere.constantsBuffer);
            efg.BindConstantBuffer(3, materialBuffer);
            efg.DrawIndexedInstanced(square.indexCount, 1);

            efg.BindVertexBuffer(cube.vertexBuffer);
            efg.BindIndexBuffer(cube.indexBuffer);
            //efg.Bind2DTexture(6, textureBox);
            efg.BindConstantBuffer(1, cube.transformBuffer);
            efg.BindConstantBuffer(2, cube.constantsBuffer);
            efg.BindConstantBuffer(3, cubeMaterialBuffer);
            efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

            efg.BindVertexBuffer(plane.vertexBuffer);
            efg.BindIndexBuffer(plane.indexBuffer);
            //efg.Bind2DTexture(6, texture2);
            efg.BindConstantBuffer(1, plane.transformBuffer);
            efg.BindConstantBuffer(2, plane.constantsBuffer);
            efg.BindConstantBuffer(3, planeMaterialBuffer);
            efg.DrawIndexedInstanced(planeShape.indexCount, 1);

            efg.BindVertexBuffer(cube2.vertexBuffer);
            efg.BindIndexBuffer(cube2.indexBuffer);
            efg.BindConstantBuffer(1, cube2.transformBuffer);
            efg.BindConstantBuffer(2, cube2.constantsBuffer);
            efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

            efg.BindVertexBuffer(cube3.vertexBuffer);
            efg.BindIndexBuffer(cube3.indexBuffer);
            efg.BindConstantBuffer(1, cube3.transformBuffer);
            efg.BindConstantBuffer(2, cube3.constantsBuffer);
            efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

            efg.BindVertexBuffer(cube4.vertexBuffer);
            efg.BindIndexBuffer(cube4.indexBuffer);
            efg.BindConstantBuffer(1, cube4.transformBuffer);
            efg.BindConstantBuffer(2, cube4.constantsBuffer);
            efg.DrawIndexedInstanced(cubeShape.indexCount, 1);

            //efg.BindConstantBuffer(2, sphereInstanced.constantsBuffer);
            //efg.DrawIndexedInstanced(square.indexCount, 2000);

            //for (size_t m = 0; m < mesh.materialBatches.size(); m++)
            //{
            //    EfgInstanceBatch instances = mesh.materialBatches[m];
            //    if(mesh.textures[m].diffuse_map.handle > 0)
            //        efg.Bind2DTexture(6, mesh.textures[m].diffuse_map);
            //    efg.BindConstantBuffer(2, mesh.constantsBuffer);
            //    efg.BindConstantBuffer(3, mesh.materialBuffers[m]);
            //    efg.BindVertexBuffer(instances.vertexBuffer);
            //    efg.BindIndexBuffer(instances.indexBuffer);
            //    efg.DrawIndexedInstanced(instances.indexCount);
            //}
        }

        //efg.SetPipelineState(skyboxPso);
        //efg.BindRootDescriptorTable(skybox_rootSignature);
        //efg.BindVertexBuffer(skyboxVertexBuffer);
        //efg.BindIndexBuffer(skyboxIndexBuffer);
        //efg.DrawIndexedInstanced(skybox.indexCount);

        efg.Copy2DTextureToBackbuffer(colorBuffer);

        efg.Render();
    }

    efgDestroyWindow(efgWindow);
    efg.Destroy();

    return 0;
}

