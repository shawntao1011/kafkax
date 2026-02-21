#include "kafkax/decoder.h"
#include <string.h>

int kafkax_decoder_abi_version(void) {
    return KAFKAX_DECODER_ABI_VERSION;
}

int kafkax_passthrough_decode(const kafkax_envelope_t* env,
                          kafkax_decode_out_t* out) {
    if (!env || !out || !out->buf) {
        return -1;
    }

    if (env->payload.len > out->cap) {
        out->kind = KAFKAX_DECODE_NEED_MORE;
        out->need = env->payload.len;
        out->err_msg[0] = '\0';
        return 0;
    }

    if (env->payload.len > 0) {
        memcpy(out->buf, env->payload.data, env->payload.len);
    }

    out->kind = KAFKAX_DECODE_OK;
    out->len = env->payload.len;
    out->need = 0;
    out->err_msg[0] = '\0';
    return 0;
}

/* Optional ABI anchor: compile-time signature enforcement. */
const kafkax_decode_fn kafkax_default_decoder = kafkax_passthrough_decode;