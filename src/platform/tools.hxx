#ifndef NANOLIVELENS_PLATFORM_TOOLS_HXX
#define NANOLIVELENS_PLATFORM_TOOLS_HXX
#include <string>
#include <filesystem>
#include "defs.h"

stdf::path getExecPath();
std::string exec(const std::string_view execname);

#endif //NANOLIVELENS_PLATFORM_TOOLS_HXX
