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

    efgCreateBuffer(efg, triangleVertices, sizeof(triangleVertices));

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgUpdate(efg);
    }

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
