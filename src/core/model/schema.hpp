#pragma once

#include <string_view>

#include <reflection/metadata.hpp>

namespace worm::core
{
  class Schema
  {
  public:
    constexpr Schema() noexcept = default;

    constexpr explicit Schema(std::string_view name) noexcept
      : name_{name}
    {}

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
      return name_.empty();
    }

    constexpr auto operator<=>(const Schema&) const noexcept = default;

  private:
    std::string_view name_{};
  };

  class Table
  {
  public:
    constexpr Table() noexcept = default;

    constexpr explicit Table(std::string_view name) noexcept
      : name_{name}
    {}

    constexpr explicit Table(Schema schema, std::string_view name) noexcept
      : schema_{schema},
        name_{name}
    {}

    [[nodiscard]]
    constexpr Schema schema() const noexcept
    {
      return schema_;
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
      return name_.empty();
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Table& left, const Table& right) noexcept
    {
      return left.name_ == right.name_ && left.schema_ == right.schema_;
    }

  private:
    Schema schema_{};
    std::string_view name_{};
  };

  class Column : public reflection::FieldMetadata
  {
  public:
    constexpr Column() noexcept = default;

    constexpr explicit Column(std::string_view name, Table table) noexcept
      : table_(table)
    {
      columnName = name;
    }

    constexpr explicit Column(reflection::FieldMetadata data, Table table) noexcept
      : reflection::FieldMetadata{data},
        table_(table)
    {}

    [[nodiscard]]
    constexpr Table table() const noexcept
    {
      return table_;
    }

    [[nodiscard]]
    friend constexpr bool operator==(const Column& left, const Column& right) noexcept
    {
      return left.columnName == right.columnName && left.table_ == right.table_;
    }

  private:
    Table table_{};
  };

  class View
  {
  public:
    constexpr explicit View(std::string_view name) noexcept
      : name_{name}
    {}

    constexpr explicit View(Schema schema, std::string_view name) noexcept
      : schema_{schema},
        name_{name}
    {}

    [[nodiscard]]
    constexpr View definedBy(std::string_view query) const noexcept
    {
      View copy = *this;
      copy.definition_ = query;
      return copy;
    }

    [[nodiscard]]
    constexpr View asUpdatable(bool updatable = true) const noexcept
    {
      View copy = *this;
      copy.updatable_ = updatable;
      return copy;
    }

    [[nodiscard]]
    constexpr Schema schema() const noexcept
    {
      return schema_;
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr std::string_view definition() const noexcept
    {
      return definition_;
    }

    [[nodiscard]]
    constexpr bool updatable() const noexcept
    {
      return updatable_;
    }

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
      return name_.empty();
    }

    [[nodiscard]]
    friend constexpr bool operator==(const View& left, const View& right) noexcept
    {
      return left.name_ == right.name_ && left.schema_ == right.schema_;
    }

  private:
    Schema schema_{};
    std::string_view name_;
    std::string_view definition_{};
    bool updatable_ = false;
  };
} // namespace worm::core
