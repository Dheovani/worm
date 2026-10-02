#pragma once

#include <core/query/parameter-value.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace worm::core
{
  enum class ColumnTypeKind
  {
    Boolean,
    Int16,
    Int32,
    Int64,
    Float32,
    Float64,
    Decimal,
    String,
    Enum,
    Binary,
    Date,
    Time,
    DateTime,
    Uuid,
    Json,
    Unknown
  };

  template <ColumnTypeKind T>
  struct ColumnTypeTraits;

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Boolean>
  {
    using Type = bool;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Int16>
  {
    using Type = std::int16_t;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Int32>
  {
    using Type = std::int32_t;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Int64>
  {
    using Type = std::int64_t;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Float32>
  {
    using Type = float;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Float64>
  {
    using Type = double;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Decimal>
  {
    using Type = Decimal;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::String>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Enum>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Binary>
  {
    using Type = Binary;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Date>
  {
    using Type = std::chrono::sys_days;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Time>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::DateTime>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Uuid>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Json>
  {
    using Type = std::string;
  };

  template <>
  struct ColumnTypeTraits<ColumnTypeKind::Unknown>
  {
    using Type = void;
  };

  template <ColumnTypeKind T>
  using ColumnType_t = typename ColumnTypeTraits<T>::Type;
} // namespace worm::core
