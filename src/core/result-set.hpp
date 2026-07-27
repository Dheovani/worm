#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <core/expression.hpp>

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

    explicit ResultSet(std::vector<ResultRow> rows);

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    std::size_t rowCount() const noexcept;

    [[nodiscard]]
    const std::vector<ResultRow>& rows() const noexcept;

    [[nodiscard]]
    Iterator begin() const noexcept;

    [[nodiscard]]
    Iterator end() const noexcept;

  private:
    std::vector<ResultRow> rows_;
  };

} // namespace worm::core
