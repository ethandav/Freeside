#include "stdafx.h"
#include "D3D12HelloWindow.h"
#include "efg.h"

/*
_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    D3D12HelloWindow sample(1280, 720, L"D3D12 Hello Window");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
*/

int main()
{
    EfgWindow efgWindow = efgCreateWindow(1280, 720, L"New Window");
    EfgContext efg = efgCreateContext(efgWindow);

    efgDestroyWindow(efgWindow);
    efgDestroyContext(efg);

    return 0;
}
