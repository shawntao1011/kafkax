#pragma once
#include <variant>
#include <string>
#include <chrono>
#include <cstdint>

#include "kafkax/qipc/types.hpp"

namespace kafkax::qipc {

template <class Row>
struct ColumnSpec {
    struct SymMember { std::string Row::* ptr; };
    struct TsMember  { std::int64_t Row::* ptr; };
    struct F64Member { double Row::* ptr; };
    struct I64Member { std::int64_t Row::* ptr; };
    struct I32Member { std::int32_t Row::* ptr; };

    const char* name;
    QType type;

    std::variant<SymMember, TsMember, F64Member, I64Member, I32Member> member;

    auto sym_member() const { return std::get<SymMember>(member).ptr; }
    auto ts_member()  const { return std::get<TsMember>(member).ptr; }
    auto f64_member() const { return std::get<F64Member>(member).ptr; }
    auto i64_member() const { return std::get<I64Member>(member).ptr; }
    auto i32_member() const { return std::get<I32Member>(member).ptr; }
};

// Macro helpers
#define KAFKAX_QIPC_COL_SYM(RowT, colname, member) \
    ::kafkax::qipc::ColumnSpec<RowT>{ colname, ::kafkax::qipc::QType::Sym, \
      ::kafkax::qipc::ColumnSpec<RowT>::SymMember{ &RowT::member } }

#define KAFKAX_QIPC_COL_TSNS(RowT, colname, member) \
    ::kafkax::qipc::ColumnSpec<RowT>{ colname, ::kafkax::qipc::QType::TimestampNs, \
      ::kafkax::qipc::ColumnSpec<RowT>::TsMember{ &RowT::member } }

#define KAFKAX_QIPC_COL_F64(RowT, colname, member) \
    ::kafkax::qipc::ColumnSpec<RowT>{ colname, ::kafkax::qipc::QType::Float64, \
      ::kafkax::qipc::ColumnSpec<RowT>::F64Member{ &RowT::member } }

#define KAFKAX_QIPC_COL_I64(RowT, colname, member) \
    ::kafkax::qipc::ColumnSpec<RowT>{ colname, ::kafkax::qipc::QType::Int64, \
      ::kafkax::qipc::ColumnSpec<RowT>::I64Member{ &RowT::member } }

#define KAFKAX_QIPC_COL_I32(RowT, colname, member) \
    ::kafkax::qipc::ColumnSpec<RowT>{ colname, ::kafkax::qipc::QType::Int32, \
      ::kafkax::qipc::ColumnSpec<RowT>::I32Member{ &RowT::member } }

} // namespace kafkax::qipc
