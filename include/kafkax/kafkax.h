#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    // demo / sugar
    int kafkax_subscribe(const char* topic);

    // full form
    int kafkax_subscribe_with_decoder(
        const char* topic,
        const char* decoder_lib,
        const char* decoder_fn
    );

#ifdef __cplusplus
}
#endif
