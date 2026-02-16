#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <string_view>

#include "writer.hpp"
#include "reflect.hpp"

namespace kafkax::qipc {

// Rough size estimate to reduce reallocs (kept conservative and cheap).
inline std::size_t estimate_ipc_table_size(std::size_t nrows,
                                          std::span<const std::string_view> col_names,
                                          std::size_t per_cell_bytes_hint = 16) {
    std::size_t s = 8; // ipc header
    s += 2; // table hdr
    s += 1; // dict begin
    // keys sym list: hdr(6) + strings
    s += 6;
    for (auto n : col_names) s += n.size() + 1;
    // values general list hdr(6)
    s += 6;
    // each column typed list hdr(6) + payload
    s += col_names.size() * 6;
    s += nrows * col_names.size() * per_cell_bytes_hint;
    return s;
}

template <class Row>
inline std::vector<std::uint8_t> encode_table_ipc(std::span<const Row> rows,
                                                  std::span<const ColumnSpec<Row>> cols)
{
    Builder b;

    // Avoid allocating a separate vector<string_view> for names:
    // write the sym list directly from ColumnSpec.
    std::vector<std::string_view> col_names;
    col_names.reserve(cols.size());
    for (auto& c : cols) col_names.emplace_back(c.name);

    b.begin_ipc(estimate_ipc_table_size(rows.size(), col_names, 16));

    // table = xT( xD( keys; values ) )
    emit_table_begin(b.buf);
    emit_dict_begin(b.buf);

    // keys: symbol list of column names
    emit_sym_list_hdr(b.buf, static_cast<std::uint32_t>(cols.size()));
    for (auto& c : cols) put_cstr(b.buf, std::string_view(c.name));

    // values: general list with N columns; each is a typed list length = rows.size()
    emit_general_list_hdr(b.buf, static_cast<std::uint32_t>(cols.size()));
    const std::uint32_t n = static_cast<std::uint32_t>(rows.size());

    for (auto& c : cols) {
        switch (c.type) {
            case QType::Sym: {
                emit_typed_list_hdr(b.buf, KS_LIST, n);
                const auto m = c.sym_member();
                for (auto& r : rows) put_cstr(b.buf, std::string_view(r.*m));
                break;
            }
            case QType::TimestampNs: {
                emit_typed_list_hdr(b.buf, KP_TS, n);
                const auto m = c.ts_member();
                for (auto& r : rows) put_i64_le(b.buf, to_unix_ns(r.*m));
                break;
            }
            case QType::Float64: {
                emit_typed_list_hdr(b.buf, KF_LIST, n);
                const auto m = c.f64_member();
                for (auto& r : rows) put_f64_le(b.buf, r.*m);
                break;
            }
            case QType::Int64: {
                emit_typed_list_hdr(b.buf, KJ_LIST, n);
                const auto m = c.i64_member();
                for (auto& r : rows) put_i64_le(b.buf, r.*m);
                break;
            }
            case QType::Int32: {
                emit_typed_list_hdr(b.buf, KI_LIST, n);
                const auto m = c.i32_member();
                for (auto& r : rows) put_i32_le(b.buf, r.*m);
                break;
            }
        }
    }

    return b.to_bytes();
}

} // namespace kafkax::qipc
