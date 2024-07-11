#include "efg_window.h"

EfgWindow efgCreateWindow(uint32_t width, uint32_t height, const wchar_t* windowTitle)
{
	EfgWindow window = {};
    EfgWindowInternal* efgWindow = new EfgWindowInternal(window);
    efgWindow->initialize(window, width, height, windowTitle);
	return window;
}

void efgDestroyWindow(EfgWindow window)
{
    EfgWindowInternal* efgWindow = EfgWindowInternal::GetEfgWindow(window);
    if (efgWindow)
    {
        delete efgWindow;
    }
}

void efgWindowPumpEvents(EfgWindow window)
{
    EfgWindowInternal* efgWindow = EfgWindowInternal::GetEfgWindow(window);
    efgWindow->pumpEvents();
}

void EfgWindowInternal::pumpEvents()
{
    MSG msg = {};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool efgWindowIsRunning(EfgWindow window)
{
    EfgWindowInternal* efgWindow = EfgWindowInternal::GetEfgWindow(window);
    return efgWindow->active;
}

void EfgWindowInternal::initialize(EfgWindow& window, uint32_t width, uint32_t height, const wchar_t* windowTitle)
{
	    windowTitle = (!windowTitle ? L"Freeside" : windowTitle);

        WNDCLASSEX window_class    = {};
        window_class.cbSize        = sizeof(window_class);
        window_class.style         = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc   = WindowProc;
        window_class.hInstance     = GetModuleHandle(nullptr);
        window_class.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        window_class.lpszClassName = windowTitle;

        RegisterClassEx(&window_class);

        RECT window_rect = { 0, 0, (LONG)width, (LONG)height };

        DWORD const window_style = WS_OVERLAPPEDWINDOW;

        AdjustWindowRect(&window_rect, window_style, FALSE);

        window_ = CreateWindowEx(
            0,
            windowTitle,
            windowTitle,
            window_style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_rect.right - window_rect.left,
            window_rect.bottom - window_rect.top,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        SetWindowLongPtrA(window_, GWLP_USERDATA, (LONG_PTR)this);

        ShowWindow(window_, SW_SHOWDEFAULT);

        window.hWnd = window_;
        active = true;
}

// Main message handler for the sample.
LRESULT CALLBACK EfgWindowInternal::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    EfgWindowInternal* efgWindow = (EfgWindowInternal*)GetWindowLongPtrA(hWnd, GWLP_USERDATA);
    switch (message)
    {
        case WM_DESTROY:
        {
            efgWindow->active = false;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

EfgWindowInternal* EfgWindowInternal::GetEfgWindow(EfgWindow& window)
{
	return reinterpret_cast<EfgWindowInternal*>(window.handle);
}
