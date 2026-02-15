#include "kafkax/decoder.h"

int kafkax_decoder_abi_version() {
    return KAFKAX_DECODER_ABI_VERSION;
}

/* Reference decoder: passthrough raw payload. */
int kafkax_default_decode(const kafkax_envelope_t* env,
                          kafkax_decode_result_t* out) {
    out->kind = 0;
    out->bytes = env->payload;
    out->len = env->payload_len;
    out->err_msg[0] = '\0';
    return 0;
}
