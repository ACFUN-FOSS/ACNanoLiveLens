/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_SPECIAL_IO
#define ESS_SPECIAL_IO
#include <filesystem>

namespace Essentials::IO
{

namespace stdf = std::filesystem;

std::string readFile(const stdf::path& path);

}

#endif //ESS_SPECIAL_IO