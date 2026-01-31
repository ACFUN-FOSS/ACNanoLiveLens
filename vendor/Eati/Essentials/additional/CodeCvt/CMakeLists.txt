cmake_minimum_required(VERSION 3.5)
project(EatiEssentials_CodeCvt LANGUAGES CXX VERSION 0.1)

find_package(Iconv REQUIRED)
fetch_header_only_library(https://raw.githubusercontent.com/unnonouno/iconvpp/refs/heads/master/iconv.hpp iconvpp)

add_library(EatiEssentials_CodeCvt
    private/codecvt.cxx
)
target_sources(EatiEssentials_CodeCvt PUBLIC
    FILE_SET HEADERS
    BASE_DIRS public/
    FILES
        public/EatiEssentials/codecvt.hxx
)
target_link_libraries(EatiEssentials_CodeCvt PRIVATE
	Iconv::Iconv
	iconvpp
)
add_library(EatiEssentials::CodeCvt ALIAS EatiEssentials_CodeCvt)