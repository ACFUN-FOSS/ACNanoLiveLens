/*
 * Copyright (c) 2026 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_ASSERT_HXX
#define ESS_ASSERT_HXX

#if defined(EESS_USE_CUSTOM_ASSERT_HANDLER)

[[noreturn]] void eatiEssentialsAssertHandler(const char *expr, const char *file, int line);

#ifndef NDEBUG
#define ESS_ASSERT(expr) \
	do { \
		if (!(expr)) { \
			::eatiEssentialsAssertHandler(#expr, __FILE__, __LINE__); \
		} \
	} while (false)
#else
#define ESS_ASSERT(expr) static_cast<void>(0)
#endif

#else

#include <cassert>

#define ESS_ASSERT(expr) assert(expr)

#endif

#endif // ESS_ASSERT_HXX
