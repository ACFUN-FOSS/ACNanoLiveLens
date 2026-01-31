/*
 * Copyright (c) 2024 nimshab etWeopd glog aFiRiKaj woiutsu inHLangANo (EATI)
 */
#ifndef ESS_CODECVT_HXX
#define ESS_CODECVT_HXX
#include <stdexcept>
#include <string>
#include <string_view>

namespace Essentials::String
{

class UnsupportedCodepageExcp : public std::runtime_error
{
public:
    UnsupportedCodepageExcp(unsigned int codepage);
};

enum class Encoding
{
    // C
    UTF_8,
    GB18030,
    GBK,
    BIG5,
    BIG5_HKSCS,
    ISO_2022_CN, ISO_2022_CN_EXT,
    // J
    SHIFT_JIS,
    EUC_JP,
    ISO_2022_JP,
    // K
    EUC_KR,
    CP949, ISO_2022_KR
};

std::string_view encodingEnumToStr(const Encoding encoding);

std::string convertToUTF8(const std::string_view str, const Encoding encoding);

std::string convertToAnyMultibyteEncoding(const std::string_view str, const Encoding inEncoding, const Encoding outEncoding);

std::wstring utf8ToUTF16(const std::string_view str);

// 将 UTF8 字符串转换为调用系统 API 需要使用的编码
// 对 Windows 平台，转换为系统默认 ANSI 编码
// 对 Unix 平台，转换为 UTF-8 编码
std::string utf8ToSysApiEncoding(const std::string_view str);

}


#endif //ESS_CODECVT_HXX