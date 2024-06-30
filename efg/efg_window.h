#pragma once
#include "efg.h"


class EfgWindow
{
public:
	inline operator HWND() const { return hWnd; }
private:
	friend class EfgWindowInternal;
	HWND hWnd;
	uint64_t handle;
};

class EfgWindowInternal
{
public:
	EfgWindowInternal(EfgWindow& window) { window.handle = reinterpret_cast<uint64_t>(this); }
	void initialize(EfgWindow &window, uint32_t width, uint32_t height, const wchar_t* title);
	void pumpEvents();
	static inline EfgWindowInternal* GetEfgWindow(EfgWindow& window);

	bool active = false;
private:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	HWND window_ = {};
};

EfgWindow efgCreateWindow(uint32_t width, uint32_t height, const wchar_t* name = nullptr);
void efgDestroyWindow(EfgWindow window);
bool efgWindowIsRunning(EfgWindow window);
void efgWindowPumpEvents(EfgWindow window);
