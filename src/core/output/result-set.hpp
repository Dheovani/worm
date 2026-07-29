#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <core/query/expression.hpp>

namespace worm::core
{

  struct ResultColumn
  {
    std::string name;
    Parameter value;
  };

  struct ResultRow
  {
    std::vector<ResultColumn> columns;

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    std::size_t columnCount() const noexcept;
  };

  class ResultSet
  {
  public:
    using Iterator = std::vector<ResultRow>::const_iterator;

    ResultSet() = default;

    explicit ResultSet(std::vector<ResultRow> rows, std::uint64_t affectedRows = 0);

    explicit ResultSet(std::uint64_t affectedRows);

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    std::size_t rowCount() const noexcept;

    [[nodiscard]]
    std::uint64_t affectedRows() const noexcept;

    [[nodiscard]]
    const std::vector<ResultRow>& rows() const noexcept;

    [[nodiscard]]
    Iterator begin() const noexcept;

    [[nodiscard]]
    Iterator end() const noexcept;

  private:
    std::vector<ResultRow> rows_;
    std::uint64_t affectedRows_{0};
  };

} // namespace worm::core
