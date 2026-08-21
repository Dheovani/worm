#include <connection/schema-inspector.hpp>

#include <core/output/result-set.hpp>

#include <iostream>
#include <utility>

namespace
{
  class FakeClient final : public worm::connection::Client
  {
  public:
    explicit FakeClient(worm::core::ResultSet result)
      : result_(std::move(result))
    {}

    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::PostgreSQL;
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
  };

  worm::core::ResultRow columnRow(
    std::string column, std::int64_t nullable, std::int64_t generated, std::int64_t unique, std::int64_t primaryKey)
  {
    return {{
      {"schema_name", std::string{"public"}},
      {"table_name", std::string{"users"}},
      {"column_name", std::move(column)},
      {"is_nullable", nullable},
      {"is_generated", generated},
      {"is_unique", unique},
      {"is_primary_key", primaryKey},
    }};
  }
} // namespace

int main()
{
  FakeClient client{worm::core::ResultSet{{
    columnRow("id", 0, 1, 0, 1),
    columnRow("email", 0, 0, 1, 0),
  }}};

  const worm::connection::SchemaInspector inspector{client};
  const worm::core::SchemaSnapshot schema = inspector.inspect();
  const auto* users = schema.findTable("public", "users");

  if (users == nullptr || users->columns.size() != 2 || users->primaryKey != std::vector<std::string>{"id"} ||
      users->columns[0].nullable || !users->columns[0].generated || !users->columns[1].unique ||
      client.statement().sql.find("information_schema.columns") == std::string::npos) {
    std::cerr << "SchemaInspector did not normalize driver metadata.\n";
    return 1;
  }

  return 0;
}
