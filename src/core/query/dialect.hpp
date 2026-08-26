#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <core/model/schema.hpp>

namespace worm::core
{

  class Dialect
  {
  public:
    virtual ~Dialect() = default;

    [[nodiscard]]
    virtual std::string placeholder(std::size_t index) const = 0;

    [[nodiscard]]
    virtual std::string quoteIdentifier(std::string_view identifier) const = 0;

    [[nodiscard]]
    virtual std::string renderColumnType(const ColumnType& type) const = 0;
  };

  class PostgresDialect : public Dialect
  {
  public:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;

    [[nodiscard]]
    std::string quoteIdentifier(std::string_view identifier) const override;

    [[nodiscard]]
    std::string renderColumnType(const ColumnType& type) const override;
  };

  class MySqlDialect : public Dialect
  {
  public:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;

    [[nodiscard]]
    std::string quoteIdentifier(std::string_view identifier) const override;

    [[nodiscard]]
    std::string renderColumnType(const ColumnType& type) const override;
  };

  class SqliteDialect : public Dialect
  {
  public:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;

    [[nodiscard]]
    std::string quoteIdentifier(std::string_view identifier) const override;

    [[nodiscard]]
    std::string renderColumnType(const ColumnType& type) const override;
  };

  class SqlServerDialect : public Dialect
  {
  public:
    [[nodiscard]]
    std::string placeholder(std::size_t index) const override;

    [[nodiscard]]
    std::string quoteIdentifier(std::string_view identifier) const override;

    [[nodiscard]]
    std::string renderColumnType(const ColumnType& type) const override;
  };

} // namespace worm::core
