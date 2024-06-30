#pragma once
#include <Windows.h>
#include <cstdint>

#include "efg_window.h"

class EfgContext
{
public:
	friend class EfgInternal;
private:
	uint64_t handle;
};

class EfgInternal
{
public:
	EfgInternal(EfgContext& efg) { efg.handle = reinterpret_cast<uint64_t>(this); };
	void initialize(HWND window);
	static inline EfgInternal* GetEfg(EfgContext& context);
private:
	HWND window_ = {};
};

EfgContext efgCreateContext(HWND window);
void efgDestroyContext(EfgContext context);
