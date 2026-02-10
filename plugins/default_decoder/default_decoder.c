#include "kafkax/decoder.h"
#include <string.h>

int kafkax_decoder_abi_version() {
    return KAFKAX_DECODER_ABI_VERSION;
}

int kafkax_default_decode(
    const kafkax_envelope_t* env,
    kafkax_decode_result_t* out
) {
    out->kind = 0;
    out->msg_type = 0; // RAW
    out->out_bytes = env->value;
    out->out_len = env->value_len;
    out->err_msg[0] = '\0';
    return 0;
}
