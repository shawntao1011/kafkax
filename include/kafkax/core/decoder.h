// include/kafkax/decoder.h  (v2-only)
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAFKAX_DECODER_ABI_VERSION 2

/* ----------- Common result kinds ----------- */
typedef enum kafkax_decode_kind_t {
    KAFKAX_DECODE_OK = 0,
    KAFKAX_DECODE_ERR = 1,
    KAFKAX_DECODE_NEED_MORE = 2,
    KAFKAX_DECODE_SKIP = 3
} kafkax_decode_kind_t;

/* ----------- Envelope (view) ----------- */
/* The host owns the underlying memory (rdkafka message). Decoder must treat all pointers as read-only views. */
typedef struct kafkax_bytes_view_t {
    const uint8_t* data;
    size_t len;
} kafkax_bytes_view_t;

typedef struct kafkax_str_view_t {
    const char* data;   /* not necessarily null-terminated */
    size_t len;
} kafkax_str_view_t;

typedef struct kafkax_envelope_t {
    /* topic name (view) */
    kafkax_str_view_t topic;

    /* optional: partition/offset/timestamp */
    int32_t partition;
    int64_t offset;
    int64_t timestamp_ms;   /* -1 if unavailable */

    /* key/payload (view) */
    kafkax_bytes_view_t key;
    kafkax_bytes_view_t payload;

    /* optional: precomputed symbol (view) */
    kafkax_str_view_t symbol;

    /* opaque pointer for host context (e.g. rd_kafka_message_t*), decoder must not touch unless agreed */
    const void* opaque;

} kafkax_envelope_t;

/* ----------- Decode output (caller buffer) ----------- */
typedef struct kafkax_decode_out_t {
    kafkax_decode_kind_t kind;

    /* caller-provided output buffer */
    uint8_t* buf;
    size_t cap;

    /* decoder sets len when OK */
    size_t len;

    /* decoder sets need when NEED_MORE */
    size_t need;

    /* decoder sets err_msg when ERR (optional when OK) */
    char err_msg[256];
} kafkax_decode_out_t;


/* ----------- Plugin exports ----------- */
int kafkax_decoder_abi_version(void);

/* Standardized entrypoint symbol name for dlsym */
typedef int (*kafkax_decode_fn)(
    const kafkax_envelope_t* env,
    kafkax_decode_out_t* out
);

/* ----------- Built-in decoders ----------- */
int kafkax_passthrough_decoder(const kafkax_envelope_t* env,
                               kafkax_decode_out_t* out);
int kafkax_default_decoder(const kafkax_envelope_t* env,
                           kafkax_decode_out_t* out);


#ifdef __cplusplus
} /* extern "C" */
#endif
