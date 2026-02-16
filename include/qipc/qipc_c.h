#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// C ABI for qipc encoding (for POD/C structs).

typedef enum kafkax_qipc_ctype {
    KAFKAX_QIPC_C_SYM       = 1, // field is `const char*`
    KAFKAX_QIPC_C_TS_NS     = 2, // field is `int64_t` unix ns
    KAFKAX_QIPC_C_F64       = 3, // field is `double`
    KAFKAX_QIPC_C_I64       = 4, // field is `int64_t`
    KAFKAX_QIPC_C_I32       = 5  // field is `int32_t`
} kafkax_qipc_ctype_t;

typedef struct kafkax_qipc_colspec_c {
    const char* name;
    kafkax_qipc_ctype_t type;
    uint32_t offset;   // offsetof(Row, field)
} kafkax_qipc_colspec_c_t;

// Encodes a q IPC table (xT) from an array of C rows.
// Output bytes are stored in a thread_local buffer owned by the library.
// The returned pointer remains valid until the next call on the same thread.
int kafkax_qipc_encode_table_c(
    const void* rows,
    uint32_t row_size,
    uint32_t nrows,
    const kafkax_qipc_colspec_c_t* cols,
    uint32_t ncols,
    const uint8_t** out_bytes,
    uint32_t* out_len);

#ifdef __cplusplus
}
#endif
