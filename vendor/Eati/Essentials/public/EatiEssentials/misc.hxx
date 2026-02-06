/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */

#ifndef ESS_MISC_HXX
#define ESS_MISC_HXX
#include <string>
#include <sstream>
#include <type_traits>
#include <cstdint>

namespace Essentials::Misc
{

template <typename T>
concept AddrLike = std::is_convertible_v<T, void *>;

template <AddrLike T>
std::string ptrToHex(T ptr) {
    std::stringstream ss;
    ss << std::hex << static_cast<void *>(ptr);
    return ss.str();
}

}

#endif
