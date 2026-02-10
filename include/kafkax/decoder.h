#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAFKAX_DECODER_ABI_VERSION 1

    typedef struct kafkax_envelope {
        const char* topic;
        int32_t     partition;
        int64_t     offset;

        const char* key;
        int32_t     key_len;

        const uint8_t* value;
        int32_t        value_len;

        int64_t ingest_ns;
        const char* schema;   // nullable
    } kafkax_envelope_t;

    typedef struct kafkax_decode_result {
        int kind;             // 0=OK, 1=ERROR
        int msg_type;         // enum from types.h

        const uint8_t* out_bytes;
        int32_t        out_len;

        char err_msg[96];
    } kafkax_decode_result_t;

    typedef int (*kafkax_decode_fn)(
        const kafkax_envelope_t* env,
        kafkax_decode_result_t* out
    );

#ifdef __cplusplus
}
#endif
