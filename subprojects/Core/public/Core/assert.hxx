#ifndef NANOLIVELENS_CORE_ASSERT_HXX
#define NANOLIVELENS_CORE_ASSERT_HXX

[[noreturn]] void assertHandler(const char *expr, const char *file, int line);


#define NLS_ASSERT(expr) \
	do { \
		if (!(expr)) { \
			::assertHandler(#expr, __FILE__, __LINE__); \
		} \
	} while (false)


#endif // NANOLIVELENS_CORE_ASSERT_HXX
