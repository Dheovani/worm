#pragma once

#include <connection/client.hpp>
#include <core/model/migration-artifact.hpp>
#include <core/model/migration-history.hpp>
#include <core/model/schema-snapshot.hpp>
#include <core/output/result-set.hpp>
#include <core/query/expression.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/statement.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{
  template <>
  class Repository<MigrationHistory> final
  {
  public:
    explicit Repository(
      std::shared_ptr<connection::Client> dbClient,
      const QueryBuilder& queryBuilder,
      std::string schema = {});

    void initialize(const SchemaSnapshot& schemaSnapshot) const;

    [[nodiscard]]
    MigrationHistory load() const;

    void addPending(const MigrationArtifact& artifact) const;

    void markApplied(std::string_view id, std::chrono::system_clock::time_point appliedAt) const;

    void markRolledBack(std::string_view id, std::chrono::system_clock::time_point rolledBackAt) const;

    void markFailed(std::string_view id, std::string_view reason) const;

    [[nodiscard]]
    static constexpr std::string_view tableName() noexcept
    {
      return "_worm_migrations";
    }

  private:
    [[nodiscard]]
    std::string qualifiedTableName() const;

    [[nodiscard]]
    ResultSet execute(const Statement& statement) const;

    void updateState(
      std::string_view id,
      std::vector<std::pair<std::string, Parameter>> fields,
      std::string_view state) const;

    std::shared_ptr<connection::Client> dbClient_;
    const QueryBuilder queryBuilder_;
    std::string schema_;
  };
} // namespace worm::core
