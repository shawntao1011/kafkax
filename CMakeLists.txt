cmake_minimum_required(VERSION 3.16)
project(kafkax LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(kafkax
        src/adpaters/q/kafkax_q.cpp
        src/core/subscribe.cpp
        src/core/decoder_registry.cpp
        src/runtime/decode_worker.cpp
)

target_include_directories(kafkax
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

add_subdirectory(plugins/default_decoder)

add_executable(subscribe_demo
        examples/subscribe_demo.cpp
)

target_link_libraries(subscribe_demo
        PRIVATE kafkax
)
