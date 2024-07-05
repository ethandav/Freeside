#include "efg.h"

int main()
{
    EfgWindow efgWindow = efgCreateWindow(1280, 720, L"New Window");
    EfgContext efg = efgCreateContext(efgWindow);

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
        { { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } },
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.2f, 0.4f, 1.0f } }
    };

    uint32_t squareIndices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

    //EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, triangleVertices, sizeof(triangleVertices), 3);
    EfgBuffer vertexBuffer = efgCreateBuffer<Vertex>(efg, squareVertices, sizeof(squareVertices), 4);
    EfgBuffer indexBuffer = efgCreateBuffer<uint32_t>(efg, squareIndices, sizeof(squareIndices), 6);
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

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
