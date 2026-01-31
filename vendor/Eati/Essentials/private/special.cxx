/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#include "EatiEssentials/special.hxx"

namespace Essentials::Special
{

void callNullptr() {
    void (*func)() = nullptr;
	// NOLINTNEXTLINE
    func();
}

void triggerDebugger() {
#ifdef _MSC_VER
    __debugbreak();
#else
#error Not implemented
#endif
}

}

