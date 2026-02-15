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
        src/core/core.cpp
        src/core/decoder_registry.cpp
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

# q adapter (optional)
if(KAFKAX_BUILD_Q)
    add_library(kafkax_q SHARED
            src/adpaters/q/kafkax_q.cpp
    )
    target_include_directories(kafkax_q
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            $ENV{QHOME}/c
    )
    target_link_libraries(kafkax_q PRIVATE kafkax_core)
endif()

add_executable(cpp_kafka_demo examples/cpp_kafka_demo.cpp)
target_include_directories(cpp_kafka_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cpp_kafka_demo PRIVATE kafkax_core)