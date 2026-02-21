#include "decoder.h"

int kafkax_decoder_abi_version() {
    return KAFKAX_DECODER_ABI_VERSION;
}

/* Reference decoder: passthrough raw payload. */
static int decode_passthrough(const rd_kafka_message_t* msg,
                          kafkax_decode_result_t* out)
{
    if (!msg || !out) {
        return -1;
    }

    out->kind = KAFKAX_DECODE_OK;
    out->bytes = (const uint8_t*)msg->payload;
    out->len = msg->len;
    out->err_msg[0] = '\0';
    return 0;
}

/* Backward-compatible alias used by earlier demo code. */
int kafkax_default_decode(const rd_kafka_message_t* msg,
                          kafkax_decode_result_t* out) {
    return decode_passthrough(msg, out);
}
/* Optional ABI anchor: compile-time signature enforcement. */
const kafkax_decode_fn kafkax_decoder_entry = kafkax_default_decode;