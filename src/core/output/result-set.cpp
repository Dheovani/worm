#include <core/output/result-set.hpp>

#include <utility>

namespace worm::core
{
  bool ResultRow::empty() const noexcept
  {
    return columns.empty();
  }

  std::size_t ResultRow::columnCount() const noexcept
  {
    return columns.size();
  }

  ResultSet::ResultSet(std::vector<ResultRow> rows)
    : rows_(std::move(rows))
  {}

  bool ResultSet::empty() const noexcept
  {
    return rows_.empty();
  }

  std::size_t ResultSet::rowCount() const noexcept
  {
    return rows_.size();
  }

  const std::vector<ResultRow>& ResultSet::rows() const noexcept
  {
    return rows_;
  }

  ResultSet::Iterator ResultSet::begin() const noexcept
  {
    return rows_.begin();
  }

  ResultSet::Iterator ResultSet::end() const noexcept
  {
    return rows_.end();
  }
} // namespace worm::core
