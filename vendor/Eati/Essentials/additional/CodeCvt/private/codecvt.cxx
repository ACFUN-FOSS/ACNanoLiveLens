#include "EatiEssentials/codecvt.hxx"
#include <cassert>
#include <format>
#include <iconv.hpp>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


namespace Essentials::String
{

UnsupportedCodepageExcp::UnsupportedCodepageExcp(unsigned int codepage) :
    std::runtime_error{
        std::format("Unsupported codepage: {}", codepage)
    } { }

std::string_view encodingEnumToStr(const Encoding encoding) {
#define MAP(enumval) case Encoding::enumval: return #enumval;
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAP2(enumval, enumstr) case Encoding::enumval: return enumstr;
    switch(encoding) {
        MAP2(UTF_8, "UTF-8");
        MAP(GB18030);
		MAP(GBK);
        MAP(BIG5);
        MAP2(BIG5_HKSCS, "BIG5-HKSCS");
        MAP2(ISO_2022_CN, "ISO-2022-CN");
        MAP2(ISO_2022_CN_EXT, "ISO-2022-CN-EXT");
        MAP2(SHIFT_JIS, "SHIFT_JIS");
        MAP2(EUC_JP, "EUC-JP");
        MAP2(ISO_2022_JP, "ISO-2022-JP");
        MAP2(EUC_KR, "EUC-KR");
        MAP(CP949);
        MAP2(ISO_2022_KR, "ISO-2022-KR");
    default:
        assert(!"民合圣诞节进口尽快设计结构");
        return "";
    }
#undef MAP2
#undef MAP
}

std::string convertToUTF8(const std::string_view str, const Encoding encoding) {
    return convertToAnyMultibyteEncoding(str, encoding, Encoding::UTF_8);
}

static std::string invokeIconvpp(const std::string_view str, const std::string_view inEncoding, const std::string_view outEncoding) {
    iconvpp::converter conv{
        std::string{ outEncoding },
        std::string{ inEncoding },
        true,
        192000
    };
    std::string output;
	std::string s{ str };
    conv.convert(s, output);
    return output;
}

std::string convertToAnyMultibyteEncoding(const std::string_view str, const Encoding inEncoding, const Encoding outEncoding) {
    return invokeIconvpp(str, encodingEnumToStr(inEncoding), encodingEnumToStr(outEncoding));
}

std::wstring utf8ToUTF16(const std::string_view str) {
    auto utf16 = invokeIconvpp(str, encodingEnumToStr(Encoding::UTF_8), "UTF-16");
    return std::wstring{ utf16.begin(), utf16.end() };
}

/**
 *`utf8ToSysEncoding` implementation for every platforms.
 */
#if defined(UNIX)
// TODO: 不完备，UNIX 不一定使用 UTF8 编码
std::string utf8ToSysApiEncoding(const std::string &str) {
    return str;
}

#elif defined(WIN32)
Encoding winCodepageIdToEncodingName(const unsigned int codepage) {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAP(codepage, encoding) case codepage: return Encoding::encoding;
    switch(codepage) {
        // C
        MAP(54936, GB18030);
        MAP(936, GBK);
        //MAP(936, GB18030);
        MAP(950, BIG5);
        // J
        MAP(932, SHIFT_JIS);
        MAP(51932, EUC_JP);
        // K
        MAP(949, CP949);

    default:
        throw UnsupportedCodepageExcp{ codepage };
    }
#undef MAP
}

std::string utf8ToSysApiEncoding(const std::string_view str) {
    unsigned int codepage = GetACP();
    auto encoding = winCodepageIdToEncodingName(codepage);
    return convertToAnyMultibyteEncoding(str, Encoding::UTF_8, encoding);
}

#else	// defined(UNIX)
#error "Implement utf8ToSysEncoding for your platform here."

#endif	// !defined(UNIX)

}
