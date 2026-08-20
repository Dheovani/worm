#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <core/model/entity-metadata.hpp>
#include <core/query/expression.hpp>

namespace worm::core
{

  struct ResultColumn
  {
    std::string name;
    Parameter value;

    friend bool operator==(const ResultColumn&, const ResultColumn&) = default;
  };

  struct ResultRow
  {
    std::vector<ResultColumn> columns;
    bool affected{false};

    [[nodiscard]]
    bool empty() const noexcept;

    [[nodiscard]]
    std::size_t columnCount() const noexcept;

    friend bool operator==(const ResultRow&, const ResultRow&) = default;
  };

  template <Model T>
  [[nodiscard]]
  T hydrate(const ResultRow& row);

  class ResultSet
  {
  public:
    using Iterator = std::vector<ResultRow>::const_iterator;

    ResultSet() = default;

    explicit ResultSet(std::vector<ResultRow> rows, std::uint64_t affectedRows = 0);

    ResultSet(const ResultSet&) = default;
    ResultSet(ResultSet&&) noexcept = default;
    ResultSet& operator=(const ResultSet&) = default;
    ResultSet& operator=(ResultSet&&) noexcept = default;

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

    template <PersistableEntity T>
    [[nodiscard]]
    std::vector<T> hydrateAll() const
    {
      std::vector<T> entities;
      entities.reserve(rows_.size());

      for (const auto& row : rows_) {
        entities.push_back(core::hydrate<T>(row));
      }

      return entities;
    }

    [[nodiscard]]
    bool operator==(const ResultSet& other) const noexcept;

  private:
    std::vector<ResultRow> rows_;
    std::uint64_t affectedRows_{0};
  };

} // namespace worm::core
