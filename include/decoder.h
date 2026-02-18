#pragma once
#include <stdint.h>
#include <librdkafka/rdkafka.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABI version for decoder plugins. */
#define KAFKAX_DECODER_ABI_VERSION 1

/* Decode result kind */
#define KAFKAX_DECODE_OK    0
#define KAFKAX_DECODE_ERROR 1

/* ================================
 *  Output: Decode Result
 * ================================ */

/* Decode output.
 *
 * On success:
 *   out->kind  = KAFKAX_DECODE_OK
 *   out->bytes = non-NULL (can be msg->payload for passthrough)
 *   out->len   = >= 0
 *
 * On error:
 *   out->kind  = KAFKAX_DECODE_ERROR
 *   out->err_msg contains a best-effort null-terminated message.
 *
 * Memory contract for out->bytes:
 * - Plugin owns the memory behind out->bytes.
 * - Core MAY copy bytes immediately after decode returns.
 * - Therefore plugin may implement bytes as:
 *     a thread_local buffer.
 *
 * Plugin MUST ensure out->bytes remains valid at least until decode returns.
 */
typedef struct kafkax_decode_result {
    int kind;               /* KAFKAX_DECODE_OK / KAFKAX_DECODE_ERROR */

    const uint8_t* bytes;   /* decoded bytes */
    int32_t len;            /* bytes length */

    char err_msg[96];       /* error message for kind==ERROR */
} kafkax_decode_result_t;


/* ================================
 *  Exported functions
 * ================================ */

/* Main decode function type.
 *
 * Return value:
 *   0  -> call succeeded (out->kind indicates OK/ERROR)
 *   !=0-> plugin internal failure (core will treat as ERROR)
 *
 * NOTE: if implemented in C++, you MUST export with `extern "C"`.
 */
typedef int (*kafkax_decode_fn)(
    const rd_kafka_message_t* msg,
    kafkax_decode_result_t* out
);


/* ================================
 *  Required Exported Symbols
 * ================================ */

/* Strongly recommended / expected:
 *   int kafkax_decoder_abi_version(void);
 *   Core will dlopen() then dlsym() this symbol and compare with KAFKAX_DECODER_ABI_VERSION.
 */
typedef int (*kafkax_decoder_abi_version_fn)(void);

#ifdef __cplusplus
}
#endif
