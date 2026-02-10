add_library(kafkax_default_decoder SHARED
        default_decoder.c
)

target_include_directories(kafkax_default_decoder
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

set_target_properties(kafkax_default_decoder PROPERTIES
        OUTPUT_NAME kafkax_default_decoder
)
