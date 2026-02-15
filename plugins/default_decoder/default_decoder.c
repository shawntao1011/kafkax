#include "../../include/decoder.h"

int kafkax_decoder_abi_version() {
    return KAFKAX_DECODER_ABI_VERSION;
}

/* Reference decoder: passthrough raw payload. */
static int decode_passthrough(const kafkax_envelope_t* env,
                          kafkax_decode_result_t* out)
{
    if (!env || !out) {
        return -1;
    }

    out->kind = KAFKAX_DECODE_OK;
    out->bytes = env->payload;
    out->len = env->payload_len;
    out->err_msg[0] = '\0';
    return 0;
}

/* Backward-compatible alias used by earlier demo code. */
int kafkax_default_decode(const kafkax_envelope_t* env,
                          kafkax_decode_result_t* out) {
    return decode_passthrough(env, out);
}