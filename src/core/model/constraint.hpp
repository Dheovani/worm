#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string_view>

#include <core/model/schema.hpp>
#include <core/query/validator.hpp>

namespace worm::core
{
  namespace detail
  {
    inline constexpr std::size_t maxConstraintColumns = 16;
    inline constexpr std::size_t maxReferentialActions = 8;

    template <typename Target, typename Source>
    constexpr std::size_t copyBounded(std::span<Target> target, Source source) noexcept
    {
      const std::size_t count = (std::min)(target.size(), source.size());

      for (std::size_t index = 0; index < count; ++index) {
        target[index] = source.begin()[static_cast<std::ptrdiff_t>(index)];
      }

      return count;
    }
  } // namespace detail

  /* Public interface for constraint types */
  struct Constraint
  {
    virtual constexpr ~Constraint() = default;

    virtual constexpr std::string_view name() const noexcept = 0;
  };

  class PrimaryKey : public Constraint
  {
  public:
    constexpr explicit PrimaryKey(std::string_view name, std::span<const Column> cols) noexcept
      : name_(name),
        columnsCount_(detail::copyBounded<Column>(columns_, cols))
    {
    }

    constexpr explicit PrimaryKey(std::string_view name, std::initializer_list<Column> cols) noexcept
      : name_(name),
        columnsCount_(detail::copyBounded<Column>(columns_, cols))
    {
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept override
    {
      return name_;
    }

    [[nodiscard]]
    constexpr std::span<const Column> columns() const noexcept
    {
      return {columns_.data(), columnsCount_};
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
      return columnsCount_ == 0;
    }

  private:
    std::string_view name_;
    std::array<Column, detail::maxConstraintColumns> columns_{};
    std::size_t columnsCount_{0};
  };

  enum class ReferentialAction
  {
    NoAction,
    Restrict,
    Cascade,
    SetNull,
    SetDefault
  };

  struct ReferentialActionEntry
  {
    Operation operation;
    ReferentialAction action;
  };

  class ForeignKey : public Constraint
  {
  public:
    constexpr explicit ForeignKey(
      std::string_view name,
      std::span<const Column> columns,
      Table referencedTable,
      std::span<const Column> referencedColumns,
      std::span<const ReferentialActionEntry> referentialActions = {}) noexcept
      : name_(name),
        referencedTable_(referencedTable),
        columnsCount_(detail::copyBounded<Column>(columns_, columns)),
        referencedColumnsCount_(detail::copyBounded<Column>(referencedColumns_, referencedColumns)),
        referentialActionsCount_(detail::copyBounded<ReferentialActionEntry>(referentialActions_, referentialActions))
    {
    }

    constexpr explicit ForeignKey(
      std::string_view name,
      std::initializer_list<Column> columns,
      Table referencedTable,
      std::initializer_list<Column> referencedColumns,
      std::initializer_list<ReferentialActionEntry> referentialActions = {}) noexcept
      : name_(name),
        referencedTable_(referencedTable),
        columnsCount_(detail::copyBounded<Column>(columns_, columns)),
        referencedColumnsCount_(detail::copyBounded<Column>(referencedColumns_, referencedColumns)),
        referentialActionsCount_(detail::copyBounded<ReferentialActionEntry>(referentialActions_, referentialActions))
    {
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept override
    {
      return name_;
    }

    [[nodiscard]]
    constexpr std::span<const Column> columns() const noexcept
    {
      return {columns_.data(), columnsCount_};
    }

    [[nodiscard]]
    constexpr Table referencedTable() const noexcept
    {
      return referencedTable_;
    }

    [[nodiscard]]
    constexpr std::span<const Column> referencedColumns() const noexcept
    {
      return {referencedColumns_.data(), referencedColumnsCount_};
    }

    [[nodiscard]]
    constexpr std::span<const ReferentialActionEntry> referentialActions() const noexcept
    {
      return {referentialActions_.data(), referentialActionsCount_};
    }

    [[nodiscard]]
    constexpr ReferentialAction referentialActionFor(Operation operation) const noexcept
    {
      for (const auto& entry : referentialActions()) {
        if (entry.operation == operation) {
          return entry.action;
        }
      }
      return ReferentialAction::NoAction;
    }

  private:
    std::string_view name_;
    Table referencedTable_;
    std::array<Column, detail::maxConstraintColumns> columns_{};
    std::array<Column, detail::maxConstraintColumns> referencedColumns_{};
    std::array<ReferentialActionEntry, detail::maxReferentialActions> referentialActions_{};
    std::size_t columnsCount_{0};
    std::size_t referencedColumnsCount_{0};
    std::size_t referentialActionsCount_{0};
  };

  enum class IndexOrder
  {
    Ascending,
    Descending
  };

  struct IndexedColumn
  {
    Column column;
    IndexOrder order{IndexOrder::Ascending};
  };

  class Index : public Constraint
  {
  public:
    constexpr explicit Index(
      std::string_view name,
      std::span<const IndexedColumn> columns,
      bool unique = false) noexcept
      : name_(name),
        columnsCount_(detail::copyBounded<IndexedColumn>(columns_, columns)),
        unique_(unique)
    {
    }

    constexpr explicit Index(
      std::string_view name,
      std::initializer_list<IndexedColumn> columns,
      bool unique = false) noexcept
      : name_(name),
        columnsCount_(detail::copyBounded<IndexedColumn>(columns_, columns)),
        unique_(unique)
    {
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept override
    {
      return name_;
    }

    [[nodiscard]]
    constexpr std::span<const IndexedColumn> columns() const noexcept
    {
      return {columns_.data(), columnsCount_};
    }

    [[nodiscard]]
    constexpr bool unique() const noexcept
    {
      return unique_;
    }

  private:
    std::string_view name_;
    std::array<IndexedColumn, detail::maxConstraintColumns> columns_{};
    std::size_t columnsCount_{0};
    bool unique_;
  };
} // namespace worm::core
