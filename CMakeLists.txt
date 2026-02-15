cmake_minimum_required(VERSION 3.16)
project(kafkax LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKACPP REQUIRED IMPORTED_TARGET rdkafka++)
pkg_check_modules(RDKAFKA   REQUIRED IMPORTED_TARGET rdkafka)

option(KAFKAX_BUILD_Q "Build q adapter (requires k.h)" OFF)

add_library(kafkax_core SHARED
        src/kafkax/core.cpp
        src/kafkax/decoder_registry.cpp
)

target_include_directories(kafkax_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(UNIX AND NOT APPLE)
    target_link_libraries(kafkax_core
            PRIVATE
            dl
            PkgConfig::RDKAFKA
    )
endif()

# Plugin: default decoder
add_subdirectory(plugins/default_decoder)

# header-only dependency: kx/k.h
add_library(kx_kdb INTERFACE)
add_library(kx::kdb ALIAS kx_kdb)

add_library(kafkax_q SHARED
        kafkax.cpp
    )
target_include_directories(kafkax_q PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_link_libraries(kafkax_q PRIVATE kafkax_core)