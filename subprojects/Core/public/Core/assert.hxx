#ifndef NANOLIVELENS_CORE_ASSERT_HXX
#define NANOLIVELENS_CORE_ASSERT_HXX

[[noreturn]] void assertHandler(const char *expr, const char *file, int line);

#ifndef NDEBUG
#define NLS_ASSERT(expr) \
	do { \
		if (!(expr)) { \
			::assertHandler(#expr, __FILE__, __LINE__); \
		} \
	} while (false)
#else
#define NLS_ASSERT(expr) static_cast<void>(0)
#endif

#endif // NANOLIVELENS_CORE_ASSERT_HXX
