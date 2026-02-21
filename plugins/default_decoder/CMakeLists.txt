add_library(kafkax_default_decoder SHARED
        default_decoder.c
)

target_link_libraries(kafkax_default_decoder
        PRIVATE
        kafkax::abi
)

set_target_properties(kafkax_default_decoder PROPERTIES
        OUTPUT_NAME kafkax_default_decoder
)