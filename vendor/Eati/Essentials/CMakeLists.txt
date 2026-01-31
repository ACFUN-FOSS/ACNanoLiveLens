cmake_minimum_required(VERSION 3.5)
project(EatiEssentials LANGUAGES CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS true)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules/)
include(eati_cmake_tools/CommonSetup)
include(eati_cmake_tools/Dependencies)
common_compiler_setup()

# == Options =====================
option(EESS_ENABLE_CODECVT "Enable codecvt functionality")
option(EESS_ENABLE_MEMSAFETY "Enable memsafety functionality")

# == Dependencies =====================
find_package(Microsoft.GSL CONFIG REQUIRED)


# == Targets =====================
add_library(EatiEssentials
    private/dummy.cxx
	private/special.cxx
	private/io.cxx
	private/container_and_view.cxx
	private/misc.cxx
)
target_sources(EatiEssentials PUBLIC
    FILE_SET HEADERS
    BASE_DIRS public/
    FILES
        public/EatiEssentials/memory.hxx
		public/EatiEssentials/special.hxx
		public/EatiEssentials/io.hxx
		public/EatiEssentials/container_and_view.hxx
		public/EatiEssentials/misc.hxx
)
target_link_libraries(EatiEssentials PUBLIC
	Microsoft.GSL::GSL
)

if(EESS_ENABLE_CODECVT)
	add_subdirectory(additional/CodeCvt)
endif()
if(EESS_ENABLE_MEMSAFETY)
	add_subdirectory(additional/MemSafety)
endif()