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
        { { -0.25f, -0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };

    EfgBuffer vertexBuffer = efgCreateBuffer(efg, triangleVertices, sizeof(triangleVertices));
    EfgProgram program = efgCreateProgram(efg, L"shaders.hlsl");
    EfgPSO pso = efgCreateGraphicsPipelineState(efg, program);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgBindVertexBuffer(efg, vertexBuffer);
        efgSetPipelineState(efg, pso);
        efgDrawInstanced(efg, 3);
        efgRender(efg);
    }

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
