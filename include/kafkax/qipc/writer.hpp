#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>
#include <chrono>

#include "types.hpp"

namespace kafkax::qipc {

// -----------------------------
// Fast little-endian appends
// -----------------------------
// Uses resize + memcpy to avoid per-byte push_back overhead.

inline void append_bytes(std::vector<std::uint8_t>& b, const void* p, std::size_t n) {
    const auto old = b.size();
    b.resize(old + n);
    std::memcpy(b.data() + old, p, n);
}

inline void put_u8(std::vector<std::uint8_t>& b, std::uint8_t v) {
    b.push_back(v);
}

inline void put_u32_le(std::vector<std::uint8_t>& b, std::uint32_t v) {
    std::uint8_t tmp[4] = {
        std::uint8_t(v & 0xFF),
        std::uint8_t((v >> 8) & 0xFF),
        std::uint8_t((v >> 16) & 0xFF),
        std::uint8_t((v >> 24) & 0xFF)
    };
    append_bytes(b, tmp, 4);
}

inline void put_u64_le(std::vector<std::uint8_t>& b, std::uint64_t v) {
    std::uint8_t tmp[8];
    for (int i = 0; i < 8; ++i) tmp[i] = std::uint8_t((v >> (8 * i)) & 0xFF);
    append_bytes(b, tmp, 8);
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
    append_bytes(b, s.data(), s.size());
    put_u8(b, 0); // NUL
}

// IPC uses unix-epoch nanoseconds for timestamp atoms/lists
inline std::int64_t to_unix_ns(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(tp.time_since_epoch()).count();
}

// -----------------------------
// IPC object headers
// -----------------------------
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

inline void emit_sym_list_hdr(std::vector<std::uint8_t>& b, std::uint32_t n) {
    emit_typed_list_hdr(b, KS_LIST, n);
}

// -----------------------------
// IPC message wrapper (8-byte header)
// -----------------------------
struct Builder {
    std::vector<std::uint8_t> buf;

    void begin_ipc(std::size_t reserve_hint = 0) {
        buf.clear();
        if (reserve_hint) buf.reserve(reserve_hint);
        buf.resize(8, 0); // reserve ipc header
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

    std::vector<std::uint8_t> to_bytes() {
        finalize_ipc_header();
        return std::move(buf);
    }
};

} // namespace kafkax::qipc
