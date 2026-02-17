cmake_minimum_required(VERSION 3.16)
project(kafkax LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RDKAFKACPP REQUIRED IMPORTED_TARGET rdkafka++)
pkg_check_modules(RDKAFKA   REQUIRED IMPORTED_TARGET rdkafka)

option(KAFKAX_BUILD_EXAMPLES "Build example subproject" ON)

add_library(kafkax_core STATIC
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


add_library(kafkax_qipc STATIC
        src/qipc/qipc_c.cpp
)
target_compile_features(kafkax_qipc PUBLIC cxx_std_20)
target_include_directories(kafkax_qipc PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Plugins
add_subdirectory(plugins/default_decoder)

# demos
if(KAFKAX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()