cmake_minimum_required(VERSION 3.5)
project(EatiEssentials_MemSafety LANGUAGES CXX VERSION 0.1)

find_package(Microsoft.GSL CONFIG REQUIRED)

add_library(EatiEssentials_MemSafety
    private/memsafety.cxx
)
target_sources(EatiEssentials_MemSafety PUBLIC
    FILE_SET HEADERS
    BASE_DIRS public/
    FILES
        public/EatiEssentials/memsafety.hxx
)
target_link_libraries(EatiEssentials_MemSafety PRIVATE
	Microsoft.GSL::GSL
)
add_library(EatiEssentials::MemSafety ALIAS EatiEssentials_MemSafety)
