#include "qipc/qipc_c.h"
#include "qipc/writer.hpp"

#include <vector>
#include <string_view>
#include <cstdint>
#include <cstring>

namespace {

static thread_local std::vector<std::uint8_t> tls_buf;

inline const std::uint8_t* field_ptr(const void* row, std::uint32_t off) {
    return static_cast<const std::uint8_t*>(row) + off;
}

} // namespace

int kafkax_qipc_encode_table_c(
    const void* rows,
    uint32_t row_size,
    uint32_t nrows,
    const kafkax_qipc_colspec_c_t* cols,
    uint32_t ncols,
    const uint8_t** out_bytes,
    uint32_t* out_len)
{
    if (!out_bytes || !out_len) return -1;
    *out_bytes = nullptr;
    *out_len = 0;

    if (!rows || row_size == 0 || !cols || ncols == 0) return -1;

    using namespace kafkax::qipc;

    Builder b;
    // simple reserve hint
    b.begin_ipc(256 + (std::size_t)nrows * (std::size_t)ncols * 16);

    emit_table_begin(b.buf);
    emit_dict_begin(b.buf);

    // keys sym list
    emit_sym_list_hdr(b.buf, ncols);
    for (uint32_t i = 0; i < ncols; ++i) {
        put_cstr(b.buf, std::string_view(cols[i].name ? cols[i].name : ""));
    }

    // values general list
    emit_general_list_hdr(b.buf, ncols);

    for (uint32_t ci = 0; ci < ncols; ++ci) {
        const auto& c = cols[ci];

        switch (c.type) {
            case KAFKAX_QIPC_C_SYM: {
                emit_typed_list_hdr(b.buf, KS_LIST, nrows);
                for (uint32_t r = 0; r < nrows; ++r) {
                    const void* row = static_cast<const std::uint8_t*>(rows) + (std::size_t)r * row_size;
                    const char* s = *reinterpret_cast<const char* const*>(field_ptr(row, c.offset));
                    put_cstr(b.buf, std::string_view(s ? s : ""));
                }
                break;
            }
            case KAFKAX_QIPC_C_TS_NS: {
                emit_typed_list_hdr(b.buf, KP_TS, nrows);
                for (uint32_t r = 0; r < nrows; ++r) {
                    const void* row = static_cast<const std::uint8_t*>(rows) + (std::size_t)r * row_size;
                    const std::int64_t v = *reinterpret_cast<const std::int64_t*>(field_ptr(row, c.offset));
                    put_i64_le(b.buf, v);
                }
                break;
            }
            case KAFKAX_QIPC_C_F64: {
                emit_typed_list_hdr(b.buf, KF_LIST, nrows);
                for (uint32_t r = 0; r < nrows; ++r) {
                    const void* row = static_cast<const std::uint8_t*>(rows) + (std::size_t)r * row_size;
                    const double v = *reinterpret_cast<const double*>(field_ptr(row, c.offset));
                    put_f64_le(b.buf, v);
                }
                break;
            }
            case KAFKAX_QIPC_C_I64: {
                emit_typed_list_hdr(b.buf, KJ_LIST, nrows);
                for (uint32_t r = 0; r < nrows; ++r) {
                    const void* row = static_cast<const std::uint8_t*>(rows) + (std::size_t)r * row_size;
                    const std::int64_t v = *reinterpret_cast<const std::int64_t*>(field_ptr(row, c.offset));
                    put_i64_le(b.buf, v);
                }
                break;
            }
            case KAFKAX_QIPC_C_I32: {
                emit_typed_list_hdr(b.buf, KI_LIST, nrows);
                for (uint32_t r = 0; r < nrows; ++r) {
                    const void* row = static_cast<const std::uint8_t*>(rows) + (std::size_t)r * row_size;
                    const std::int32_t v = *reinterpret_cast<const std::int32_t*>(field_ptr(row, c.offset));
                    put_i32_le(b.buf, v);
                }
                break;
            }
            default:
                return -1;
        }
    }

    tls_buf = b.to_bytes();
    *out_bytes = tls_buf.data();
    *out_len = (uint32_t)tls_buf.size();
    return 0;
}
