#include "efg.h"

EfgContext efgCreateContext(HWND window)
{
	EfgContext context = {};
	EfgInternal* efg = new EfgInternal(context);
	efg->initialize(window);
	return context;
}

void efgDestroyContext(EfgContext context)
{
	EfgInternal* efg = EfgInternal::GetEfg(context);
	if (efg)
	{
		delete efg;
	}
}

void EfgInternal::initialize(HWND window)
{
	window_ = window;
}

EfgInternal* EfgInternal::GetEfg(EfgContext& context)
{
	return reinterpret_cast<EfgInternal*>(context.handle);
}
