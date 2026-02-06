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

# == Dependencies =====================
find_package(Microsoft.GSL CONFIG REQUIRED)


# == Targets =====================
add_library(EatiEssentials
    private/dummy.cxx
	private/memory.cxx
	private/memsafety.cxx
	private/special.cxx
	private/io.cxx
	private/container_and_view_and_range/container_and_view_and_range.cxx
	private/container_and_view_and_range/addandremoveable_container_ref.cxx
	private/container_and_view_and_range/container_lease.cxx
	private/misc.cxx
)
target_sources(EatiEssentials PUBLIC
    FILE_SET HEADERS
    BASE_DIRS public/
    FILES
        public/EatiEssentials/memory.hxx
		public/EatiEssentials/memsafety.hxx
		public/EatiEssentials/special.hxx
		public/EatiEssentials/io.hxx
		public/EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx
		public/EatiEssentials/container_and_view_and_ranges/addandremoveable_container_ref.hxx
		public/EatiEssentials/container_and_view_and_ranges/container_lease.hxx
		public/EatiEssentials/misc.hxx
)
target_link_libraries(EatiEssentials PUBLIC
	Microsoft.GSL::GSL
)

if(EESS_ENABLE_CODECVT)
	add_subdirectory(additional/CodeCvt)
endif()

# This MUST be called after `find_package(EatiEssentials CONFIG REQUIRED COMPONENT xxxx)`
# If EatiEssentials is used with `find_package`. This ensures EatiEssentials has components
# required in such case: In the develop environment of a downstream project of your library,
# your library and EatiEssentials isn't installed, instead, the downstream project gets them
# via FetchContent (FetchContent handles `find_package(EatiEssentials ...)`).
#
# This scenario is common if the downstream project wants to use a alternative version of
# EatiEssentials.
function(EatiEssentials_EnsureReallyHasComponents)
    foreach(component IN LISTS ARGN)
        if(NOT TARGET EatiEssentials::${component})
            message(FATAL_ERROR
				"Component ${component} is not available in EatiEssentials.\n"
				"> If you import EatiEssentials via FetchContent_Declare, "
				"make sure to set 'EESS_ENABLE_XXX' to ON.\n"
				"> If you installed EatiEssentials, make sure to "
				"find_package(EatiEssentials) with COMPONENT clauses."
			)
        endif()
    endforeach()
endfunction()
