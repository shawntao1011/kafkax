#pragma once
#include <cstdint>

namespace kafkax::qipc {

// IPC type codes (subset needed for table encoding)
inline constexpr std::uint8_t XT_TABLE = 98;  // 'xT'
inline constexpr std::uint8_t XD_DICT  = 99;  // 'xD'
inline constexpr std::uint8_t KL_LIST  = 0;   // general list
inline constexpr std::uint8_t KS_LIST  = 11;  // symbol list
inline constexpr std::uint8_t KP_TS    = 12;  // timestamp (nanoseconds since epoch)
inline constexpr std::uint8_t KF_LIST  = 9;   // float (double)
inline constexpr std::uint8_t KJ_LIST  = 7;   // long (int64)
inline constexpr std::uint8_t KI_LIST  = 6;   // int (int32)

enum class QType : std::uint8_t {
    Sym,
    TimestampNs, // int64 unix ns
    Float64,
    Int64,
    Int32
};

} // namespace kafkax::qipc
