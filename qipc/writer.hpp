#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#include "qipc/types.hpp"

namespace qipc {

// -------- little-endian writers --------
inline void put_u8(std::vector<std::uint8_t>& b, std::uint8_t v) { b.push_back(v); }

inline void put_u32_le(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(std::uint8_t(v & 0xFF));
    b.push_back(std::uint8_t((v >> 8) & 0xFF));
    b.push_back(std::uint8_t((v >> 16) & 0xFF));
    b.push_back(std::uint8_t((v >> 24) & 0xFF));
}

inline void put_u64_le(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) b.push_back(std::uint8_t((v >> (8 * i)) & 0xFF));
}

inline void put_i32_le(std::vector<std::uint8_t>& b, std::int32_t v) {
    put_u32_le(b, static_cast<std::uint32_t>(v));
}

inline void put_i64_le(std::vector<std::uint8_t>& b, std::int64_t v) {
    put_u64_le(b, static_cast<std::uint64_t>(v));
}

inline void put_f64_le(std::vector<std::uint8_t>& b, double d) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    std::uint64_t u = 0;
    std::memcpy(&u, &d, 8);
    put_u64_le(b, u);
}

inline void put_cstr(std::vector<std::uint8_t>& b, std::string_view s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0); // NUL
}

// -------- timestamp: IPC uses unix-epoch nanoseconds --------
inline std::int64_t to_ipc_unix_ns(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(tp.time_since_epoch()).count();
}

// -------- object headers --------
inline void emit_typed_list_hdr(std::vector<std::uint8_t>& b, std::uint8_t t, std::uint32_t n) {
    put_u8(b, t);
    put_u8(b, 0); // attr
    put_u32_le(b, n);
}

inline void emit_general_list_hdr(std::vector<std::uint8_t>& b, std::uint32_t n) {
    put_u8(b, KL_LIST);
    put_u8(b, 0); // attr
    put_u32_le(b, n);
}

inline void emit_table_begin(std::vector<std::uint8_t>& b) {
    put_u8(b, XT_TABLE);
    put_u8(b, 0); // attr
}

// IMPORTANT: dict xD has NO attr byte in IPC encoding (matches -8!)
inline void emit_dict_begin(std::vector<std::uint8_t>& b) {
    put_u8(b, XD_DICT);
}

inline void emit_sym_list(std::vector<std::uint8_t>& b, const std::vector<std::string_view>& syms) {
    emit_typed_list_hdr(b, KS_LIST, static_cast<std::uint32_t>(syms.size()));
    for (auto s : syms) put_cstr(b, s);
}

// -------- builder: manages IPC header --------
struct Builder {
    std::vector<std::uint8_t> buf;

    void begin_ipc(std::size_t reserve_hint = 0) {
        buf.clear();
        if (reserve_hint) buf.reserve(reserve_hint);
        buf.resize(8, 0); // reserve header
    }

    void finalize_ipc_header() {
        // [0]=endian(1 little) [1]=msgtype(0) [2]=compress(0) [3]=reserved(0) [4..7]=size LE
        buf[0] = 1;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;

        const std::uint32_t total = static_cast<std::uint32_t>(buf.size());
        buf[4] = std::uint8_t(total & 0xFF);
        buf[5] = std::uint8_t((total >> 8) & 0xFF);
        buf[6] = std::uint8_t((total >> 16) & 0xFF);
        buf[7] = std::uint8_t((total >> 24) & 0xFF);
    }

    std::string to_string() {
        finalize_ipc_header();
        return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
    }
};

} // namespace qipc
