#include "efg.h"

int main()
{
    EfgWindow efgWindow = efgCreateWindow(1280, 720, L"New Window");
    EfgContext efg = efgCreateContext(efgWindow);

    while (efgWindowIsRunning(efgWindow))
    {
        efgWindowPumpEvents(efgWindow);
        efgUpdate(efg);
    }

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
