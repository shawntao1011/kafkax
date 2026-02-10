#include "kafkax.h"

int kafkax_subscribe(const char* topic) {
    return kafkax_subscribe_with_decoder(
        topic,
        "libkafkax_default_decoder.so",
        "kafkax_default_decode"
    );
}