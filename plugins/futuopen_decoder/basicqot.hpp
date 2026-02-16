#pragma once
#include <chrono>
#include <string>
#include <vector>
#include "qipc/reflect.hpp"
#include "qipc/encode.hpp"

struct BasicQuoteRow {
    std::string symbol;
    std::chrono::system_clock::time_point time;
    double priceSpread;
    double highPrice;
    double openPrice;
    double lowPrice;
    double curPrice;
    double lastClosePrice;
    std::int64_t volume;
    double amount; // turnover
    double turnoverRate;
    double amplitude;
    std::chrono::system_clock::time_point updateTime;
};
struct BasicQuoteBatch { std::vector<BasicQuoteRow> rows; };

#define BASICQUOTE_FIELDS(X, RowT) \
X(SYM,  "sym",         symbol) \
X(TS,   "time",        time) \
X(F64,  "spread",      priceSpread) \
X(F64,  "high",        highPrice) \
X(F64,  "open",        openPrice) \
X(F64,  "low",         lowPrice) \
X(F64,  "cur",         curPrice) \
X(F64,  "lastclose",   lastClosePrice) \
X(I64,  "volume",      volume) \
X(F64,  "amount",      amount) \
X(F64,  "turnoverrate",turnoverRate) \
X(F64,  "amplitude",   amplitude) \
X(TS,   "updtime",     updateTime)

namespace schemas {

inline const std::array<qipc::ColumnSpec<BasicQuoteRow>, 13> BASICQUOTE_COLS = {{
#define X(kind, colname, member) QIPC_COL_##kind(BasicQuoteRow, colname, member),
        BASICQUOTE_FIELDS(X, BasicQuoteRow)
    #undef X
    }};

} // namespace schemas