#include <core/model/column.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <string>

using worm::core::ColumnType_t;
using worm::core::ColumnTypeKind;

static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Boolean>, bool>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Int16>, std::int16_t>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Int32>, std::int32_t>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Int64>, std::int64_t>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Float32>, float>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Float64>, double>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Decimal>, worm::core::Decimal>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::String>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Enum>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Binary>, worm::core::Binary>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Date>, std::chrono::sys_days>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Time>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::DateTime>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Uuid>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Json>, std::string>);
static_assert(std::same_as<ColumnType_t<ColumnTypeKind::Unknown>, void>);

int main()
{
  return 0;
}
