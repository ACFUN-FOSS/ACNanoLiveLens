#ifndef NANOLIVELENS_PLATFORM_CRASH_HANDLER_HXX
#define NANOLIVELENS_PLATFORM_CRASH_HANDLER_HXX
#include <functional>
#include <string_view>


// Implement in main.cxx
int crashHandlerProtectedMain();

// Platform dependent things
void runProtectedMain();

extern bool assertFailedHandlerTriggered;

#endif //NANOLIVELENS_PLATFORM_CRASH_HANDLER_HXX
