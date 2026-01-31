cmake_minimum_required (VERSION 3.10)
project(soloudpp)

if(NOT TARGET soloud::soloud)
    message(FATAL_ERROR "Please ensure soloud::soloud target exist")
endif()

add_library(Soloudpp)
target_sources(Soloudpp
    PRIVATE
        private/Soloud.cxx
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS public/
    FILES
        public/Soloudpp/Soloud.hxx
)
target_link_libraries(Soloudpp PUBLIC soloud::soloud)

add_library(Hs::Soloudpp ALIAS Soloudpp)