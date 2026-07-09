#include "Core/assert.hxx"
#include <EatiEssentials/assert.hxx>
#include <cstdlib>
#include <iostream>

namespace {

[[noreturn]] void printAssertFailThenAbort(const char *source, const char *expr, const char *file, const int line) {
	std::cerr << source << " assert failed: " << expr << '\n'
		<< file << ':' << line << std::endl;
	std::abort();
}

}

[[noreturn]] void assertHandler(const char *expr, const char *file, const int line) {
	printAssertFailThenAbort("NanoLiveLens", expr, file, line);
}

[[noreturn]] void eatiEssentialsAssertHandler(const char *expr, const char *file, const int line) {
	printAssertFailThenAbort("EatiEssentials", expr, file, line);
}