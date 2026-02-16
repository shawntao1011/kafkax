// Example: BasicQot -> q IPC table bytes -> hex string (0x...)
// Build: g++ -O2 -std=c++20 basicqot_example.cpp -I../include -L../build -lkafkax_qipc
#include <array>
#include <vector>
#include <chrono>
#include <iostream>

#include "qipc/encode.hpp"   // C++ API
#include "qipc/reflect.hpp"

static std::string to_hex_0x(const std::vector<std::uint8_t>& bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(2 + bytes.size() * 2);
  out += "0x";
  for (std::uint8_t b : bytes) {
    out.push_back(kHex[(b >> 4) & 0xF]);
    out.push_back(kHex[b & 0xF]);
  }
  return out;
}

struct BasicQuoteRow {
  std::string sym; // "HK.00700"
  std::chrono::system_clock::time_point time;
  double cur{};
  std::int64_t volume{};
};

static inline const std::array<kafkax::qipc::ColumnSpec<BasicQuoteRow>, 4> BASICQOT_COLS = {{
  KAFKAX_QIPC_COL_SYM (BasicQuoteRow, "sym",   sym),
  KAFKAX_QIPC_COL_TSNS(BasicQuoteRow, "time",  time),
  KAFKAX_QIPC_COL_F64 (BasicQuoteRow, "cur",   cur),
  KAFKAX_QIPC_COL_I64 (BasicQuoteRow, "volume",volume),
}};

int main() {
  std::vector<BasicQuoteRow> rows;
  rows.push_back({"HK.00700", std::chrono::system_clock::now(), 300.5, 123456});

  std::vector<std::uint8_t> kbytes =
    kafkax::qipc::encode_table_ipc<BasicQuoteRow>(rows, BASICQOT_COLS);

  // kbytes is a full q IPC message (8-byte header + payload).
  // You can send it via kdb+ IPC, or wrap it into your pipeline.
  std::cout << to_hex_0x(kbytes) << "\n";

  return (kbytes.size() > 8) ? 0 : 1;
}
