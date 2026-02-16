#pragma once
#include <variant>
#include <string>
#include <string_view>
#include <chrono>

#include "qipc/types.hpp"

namespace qipc {

    template <class Row>
    struct ColumnSpec {
        using SymMember  = std::string Row::*;
        using TsMember   = std::chrono::system_clock::time_point Row::*;
        using F64Member  = double Row::*;
        using I64Member  = std::int64_t Row::*;
        using I32Member  = std::int32_t Row::*;

        const char* name;
        QType type;

        std::variant<SymMember, TsMember, F64Member, I64Member, I32Member> member;

        // helpers
        SymMember sym_member() const { return std::get<SymMember>(member); }
        TsMember  ts_member()  const { return std::get<TsMember>(member); }
        F64Member f64_member() const { return std::get<F64Member>(member); }
        I64Member i64_member() const { return std::get<I64Member>(member); }
        I32Member i32_member() const { return std::get<I32Member>(member); }
    };

    // Macro helper to build ColumnSpec entries
#define QIPC_COL_SYM(RowT, colname, member)  qipc::ColumnSpec<RowT>{ colname, qipc::QType::Sym,       qipc::ColumnSpec<RowT>::SymMember{ &RowT::member } }
#define QIPC_COL_TS(RowT, colname, member)   qipc::ColumnSpec<RowT>{ colname, qipc::QType::Timestamp, qipc::ColumnSpec<RowT>::TsMember{  &RowT::member } }
#define QIPC_COL_F64(RowT, colname, member)  qipc::ColumnSpec<RowT>{ colname, qipc::QType::Float64,   qipc::ColumnSpec<RowT>::F64Member{ &RowT::member } }
#define QIPC_COL_I64(RowT, colname, member)  qipc::ColumnSpec<RowT>{ colname, qipc::QType::Int64,     qipc::ColumnSpec<RowT>::I64Member{ &RowT::member } }
#define QIPC_COL_I32(RowT, colname, member)  qipc::ColumnSpec<RowT>{ colname, qipc::QType::Int32,     qipc::ColumnSpec<RowT>::I32Member{ &RowT::member } }

} // namespace qipc
