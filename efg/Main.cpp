#include "efg.h"

int main()
{
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

    XMMATRIX viewProj;

    //EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, triangleVertices, sizeof(triangleVertices), 3);
    EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, EFG_VERTEX_BUFFER, squareVertices, sizeof(squareVertices), 4);
    EfgBuffer indexBuffer = efgCreateBuffer<uint32_t>(efg, EFG_INDEX_BUFFER, squareIndices, sizeof(squareIndices), 6);
    EfgBuffer constantBuffer = efgCreateBuffer<XMMATRIX>(efg, EFG_CONSTANT_BUFFER, &viewProj, sizeof(viewProj), 1);
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
    efgDestroyBuffer(constantBuffer);

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
