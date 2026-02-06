/*
 * Copyright (c) 2025 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_MEMSAFETY_HXX
#define ESS_MEMSAFETY_HXX
#include <cassert>

namespace Essentials::Memory
{

// Lifetimebound attribute
#if defined(_MSC_VER) // MSVC
#ifndef __clang__   // Ignore for clang-based tools
#define LIFETIMEBOUND [[msvc::lifetimebound]]
#include <CppCoreCheck/Warnings.h>
#pragma warning(default: CPPCORECHECK_LIFETIME_WARNINGS) // Enable lifetimebound warnings
#endif  // __clang__

#elif defined(__GNUC__) // GCC
#ifndef __clang__   // Ignore for clang-based tools
#define LIFETIMEBOUND [[clang::lifetimebound]]
#endif  // __clang__

#elif defined(__clang__)
#define LIFETIMEBOUND

#endif

#ifndef LIFETIMEBOUND
#define LIFETIMEBOUND
#endif


// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define C_MOVE(x) x
#define C_OWNER_DATATYPE


// Nullability checking does not work for now
/*
#ifdef __clang__
    // NOLINTNEXTLINE
    #define PTR_PARAM(t) t *_Nullable
#else
    // NOLINTNEXTLINE
    #define PTR_PARAM(t) t*
#endif
*/

// TODO: Replace macro to function template?
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UNWRAP(x) \
    [](auto ptr) -> decltype(*ptr) & { \
        assert(ptr != nullptr && "Null pointer dereference"); \
        return *ptr; \
    }(x)

// TODO: Replace macro to function template?
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define EXCEPT(x, failMsg) \
	[&](auto ptr) -> decltype(*ptr) & { \
		std::string msg = failMsg; \
		if (!ptr) { \
			std::println(std::cerr, "EXCEPT failed: {}", msg); \
			assert(ptr != nullptr); \
		} \
		return *ptr; \
	}(x)
}
#endif
