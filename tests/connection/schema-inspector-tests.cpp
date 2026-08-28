#include <connection/schema-inspector.hpp>

#include <core/output/result-set.hpp>

#include <iostream>
#include <utility>

namespace
{
  class FakeClient final : public worm::connection::Client
  {
  public:
    explicit FakeClient(worm::core::ResultSet result, worm::connection::DatabaseType type)
      : result_(std::move(result)),
        type_(type)
    {}

    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return type_;
    }

    [[nodiscard]]
    const worm::core::Statement& statement() const noexcept
    {
      return statement_;
    }

  private:
    void beginTransactionImpl() override {}

    void rollbackTransactionImpl() override {}

    void commitTransactionImpl() override {}

    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      statement_ = statement;
      return result_;
    }

    worm::core::ResultSet result_;
    worm::core::Statement statement_;
    worm::connection::DatabaseType type_;
  };

  worm::core::ResultRow columnRow(
    std::string column,
    std::string type,
    std::string nativeType,
    std::int64_t nullable,
    std::int64_t generated,
    std::int64_t unique,
    std::int64_t primaryKey,
    worm::core::Parameter length = nullptr,
    worm::core::Parameter precision = nullptr,
    worm::core::Parameter scale = nullptr,
    std::int64_t unsignedValue = 0,
    std::int64_t withTimeZone = 0)
  {
    return {{
      {"schema_name", std::string{"public"}},
      {"table_name", std::string{"users"}},
      {"column_name", std::move(column)},
      {"is_nullable", nullable},
      {"is_generated", generated},
      {"is_unique", unique},
      {"is_primary_key", primaryKey},
      {"type_name", std::move(type)},
      {"native_type", std::move(nativeType)},
      {"type_length", std::move(length)},
      {"type_precision", std::move(precision)},
      {"type_scale", std::move(scale)},
      {"is_unsigned", unsignedValue},
      {"has_time_zone", withTimeZone},
    }};
  }
} // namespace

int main()
{
  FakeClient client{
    worm::core::ResultSet{{
      columnRow("id", "bigint", "int8", 0, 1, 0, 1, nullptr, "64", "0"),
      columnRow("email", "character varying", "varchar", 0, 0, 1, 0, "255"),
      columnRow("created_at", "timestamp with time zone", "timestamptz", 0, 0, 0, 0, nullptr, nullptr, nullptr, 0, 1),
    }},
    worm::connection::DatabaseType::PostgreSQL};

  const worm::connection::SchemaInspector inspector{client};
  const worm::core::SchemaSnapshot schema = inspector.inspect();
  const auto* users = schema.findTable("public", "users");

  if (users == nullptr || users->columns.size() != 3 || users->primaryKey != std::vector<std::string>{"id"} ||
      users->columns[0].nullable || !users->columns[0].generated || !users->columns[1].unique ||
      users->columns[0].type.kind != worm::core::ColumnTypeKind::Int64 || users->columns[0].type.precision != 64 ||
      users->columns[1].type.kind != worm::core::ColumnTypeKind::String || users->columns[1].type.length != 255 ||
      users->columns[2].type.kind != worm::core::ColumnTypeKind::DateTime || !users->columns[2].type.withTimeZone ||
      client.statement().sql.find("information_schema.columns") == std::string::npos) {
    std::cerr << "SchemaInspector did not normalize driver metadata.\n";
    return 1;
  }

  FakeClient mysql{worm::core::ResultSet{{columnRow("active", "tinyint", "tinyint(1)", 0, 0, 0, 0)}},
    worm::connection::DatabaseType::MySQL};
  FakeClient sqlite{worm::core::ResultSet{{columnRow("amount", "DECIMAL(10,2)", "DECIMAL(10,2)", 0, 0, 0, 0)}},
    worm::connection::DatabaseType::SQLite};
  FakeClient mssql{worm::core::ResultSet{{columnRow("token", "uniqueidentifier", "uniqueidentifier", 0, 0, 0, 0)}},
    worm::connection::DatabaseType::MSSQL};

  const worm::connection::SchemaInspector mysqlInspector{mysql};
  const worm::connection::SchemaInspector sqliteInspector{sqlite};
  const worm::connection::SchemaInspector mssqlInspector{mssql};
  if (mysqlInspector.inspect().tables[0].columns[0].type.kind != worm::core::ColumnTypeKind::Boolean ||
      sqliteInspector.inspect().tables[0].columns[0].type.kind != worm::core::ColumnTypeKind::Decimal ||
      mssqlInspector.inspect().tables[0].columns[0].type.kind != worm::core::ColumnTypeKind::Uuid) {
    std::cerr << "SchemaInspector did not normalize database-specific types.\n";
    return 1;
  }

  return 0;
}
