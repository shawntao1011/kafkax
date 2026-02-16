#pragma once
#include <cstdint>

namespace qipc {

    // IPC type codes (subset)
    inline constexpr std::uint8_t XT_TABLE = 98;  // 0x62
    inline constexpr std::uint8_t XD_DICT  = 99;  // 0x63
    inline constexpr std::uint8_t KL_LIST  = 0;   // general list
    inline constexpr std::uint8_t KS_LIST  = 11;  // symbol list
    inline constexpr std::uint8_t KP_TS    = 12;  // timestamp list
    inline constexpr std::uint8_t KF_LIST  = 9;   // float (double) list
    inline constexpr std::uint8_t KJ_LIST  = 7;   // long (int64) list
    inline constexpr std::uint8_t KI_LIST  = 6;   // int (int32) list

    enum class QType : std::uint8_t {
        Sym,
        Timestamp, // unix ns (IPC)
        Float64,
        Int64,
        Int32
    };

} // namespace qipc
