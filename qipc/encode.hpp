#pragma once
#include <span>
#include <vector>
#include <string_view>

#include "qipc/writer.hpp"
#include "qipc/reflect.hpp"

namespace qipc {

template <class Row>
inline std::string encode_table_ipc(std::span<const Row> rows,
                                    std::span<const ColumnSpec<Row>> cols)
{
    Builder b;
    // reserve hint: rough
    b.begin_ipc(256 + rows.size() * cols.size() * 16);

    // table = xT( xD( keys; values ) )
    emit_table_begin(b.buf);
    emit_dict_begin(b.buf);

    // keys: symbol list of column names
    std::vector<std::string_view> names;
    names.reserve(cols.size());
    for (auto& c : cols) names.emplace_back(c.name);
    emit_sym_list(b.buf, names);

    // values: general list with N columns, each is a typed list length = rows.size()
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
            case QType::Timestamp: {
                emit_typed_list_hdr(b.buf, KP_TS, n);
                const auto m = c.ts_member();
                for (auto& r : rows) put_i64_le(b.buf, to_ipc_unix_ns(r.*m));
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
            default:
                // If you add more QTypes, handle them here.
                break;
        }
    }

    return b.to_string();
}

} // namespace qipc
