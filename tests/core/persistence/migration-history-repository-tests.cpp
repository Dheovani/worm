#include <core/persistence/migration-history-repository.hpp>

#include <connection/client.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/invalid-arg-exception.hpp>
#include <errors/migration-exception.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
  class RecordingClient final : public worm::connection::Client
  {
  public:
    explicit RecordingClient(std::vector<worm::core::ResultSet> responses)
      : responses_(std::move(responses))
    {}

    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    std::vector<worm::core::Statement> statements;

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      statements.push_back(statement);
      if (nextResponse_ == responses_.size()) {
        return {};
      }
      return responses_[nextResponse_++];
    }

    void beginTransactionImpl() override {}
    void rollbackTransactionImpl() override {}
    void commitTransactionImpl() override {}

    std::vector<worm::core::ResultSet> responses_;
    std::size_t nextResponse_{0};
  };

  template <typename Type>
  std::shared_ptr<Type> nonOwning(Type& value)
  {
    return std::shared_ptr<Type>{&value, [](Type*) {}};
  }

  worm::core::MigrationArtifact migrationArtifact()
  {
    return worm::core::makeMigrationArtifact(
      "20260922143000",
      "create-users",
      "sqlite",
      {
        {
          .description = "Create users",
          .sql = "create table users (id integer primary key)",
        },
      });
  }

  worm::core::SchemaTableSnapshot historyTableSnapshot()
  {
    using worm::core::ColumnTypeKind;
    return {
      .schema = "main",
      .name = "_worm_migrations",
      .columns =
        {
          {.name = "id", .type = {.kind = ColumnTypeKind::String}, .nullable = false},
          {.name = "name", .type = {.kind = ColumnTypeKind::String}, .nullable = false},
          {.name = "checksum", .type = {.kind = ColumnTypeKind::String}, .nullable = false},
          {.name = "state", .type = {.kind = ColumnTypeKind::String}, .nullable = false},
          {.name = "applied_at", .type = {.kind = ColumnTypeKind::Int64}, .nullable = true},
          {.name = "rolled_back_at", .type = {.kind = ColumnTypeKind::Int64}, .nullable = true},
          {.name = "failure_reason", .type = {.kind = ColumnTypeKind::String}, .nullable = true},
        },
      .primaryKey = {"id"},
    };
  }

  worm::core::ResultRow appliedMigrationRow(const worm::core::MigrationArtifact& artifact)
  {
    return {
      .columns =
        {
          {"id", artifact.id()},
          {"name", artifact.name()},
          {"checksum", artifact.checksum()},
          {"state", std::string{"applied"}},
          {"applied_at", std::int64_t{1250}},
          {"rolled_back_at", nullptr},
          {"failure_reason", nullptr},
        },
    };
  }
} // namespace

int main()
{
  const worm::core::MigrationArtifact artifact = migrationArtifact();
  RecordingClient client{
    {
      worm::core::ResultSet{},
      worm::core::ResultSet{{appliedMigrationRow(artifact)}},
      worm::core::ResultSet{std::uint64_t{1}},
      worm::core::ResultSet{std::uint64_t{1}},
      worm::core::ResultSet{std::uint64_t{1}},
      worm::core::ResultSet{std::uint64_t{1}},
      worm::core::ResultSet{std::uint64_t{0}},
    },
  };
  const worm::core::SqliteBuilder sqlBuilder;
  const worm::core::QueryBuilder queryBuilder{sqlBuilder};
  const worm::core::Repository<worm::core::MigrationHistory> repository{
    nonOwning(client),
    queryBuilder,
    "main",
  };

  repository.initialize({});
  if (client.statements.size() != 1 ||
      client.statements.front().sql.find("create table \"main\".\"_worm_migrations\"") != 0) {
    std::cerr << "Migration history repository did not create missing storage.\n";
    return 1;
  }

  const worm::core::MigrationHistory history = repository.load();
  const worm::core::MigrationRecord* record = history.find(artifact.id());
  if (record == nullptr || record->name != artifact.name() || record->checksum != artifact.checksum() ||
      record->state != worm::core::MigrationState::Applied ||
      record->appliedAt != std::chrono::system_clock::time_point{std::chrono::milliseconds{1250}} ||
      record->rolledBackAt.has_value() || !record->failureReason.empty()) {
    std::cerr << "Migration history repository did not load persisted records.\n";
    return 1;
  }

  repository.addPending(artifact);
  repository.markApplied(artifact.id(), std::chrono::system_clock::time_point{std::chrono::milliseconds{2500}});
  repository.markFailed(artifact.id(), "DDL failed");
  repository.markRolledBack(artifact.id(), std::chrono::system_clock::time_point{std::chrono::milliseconds{3000}});

  const std::vector<worm::core::Parameter> expectedInsertParameters{
    artifact.id(),
    artifact.name(),
    artifact.checksum(),
    std::string{"pending"},
    nullptr,
    nullptr,
    nullptr,
  };
  const std::vector<worm::core::Parameter> expectedAppliedParameters{
    std::string{"applied"},
    std::int64_t{2500},
    nullptr,
    nullptr,
    artifact.id(),
  };
  const std::vector<worm::core::Parameter> expectedFailedParameters{
    std::string{"failed"},
    std::string{"DDL failed"},
    artifact.id(),
  };
  const std::vector<worm::core::Parameter> expectedRollbackParameters{
    std::string{"rolled_back"},
    std::int64_t{3000},
    artifact.id(),
  };
  if (client.statements.size() != 6 || client.statements[2].sql.find("insert into main._worm_migrations") != 0 ||
      client.statements[2].parameters != expectedInsertParameters ||
      client.statements[3].parameters != expectedAppliedParameters ||
      client.statements[4].parameters != expectedFailedParameters ||
      client.statements[5].parameters != expectedRollbackParameters) {
    std::cerr << "Migration history repository did not persist parameterized state transitions.\n";
    return 1;
  }

  bool missingMigrationRejected = false;
  try {
    repository.markFailed("missing", "not found");
  } catch (const worm::MigrationException&) {
    missingMigrationRejected = true;
  }
  if (!missingMigrationRejected) {
    std::cerr << "Migration history repository accepted a state transition without an affected record.\n";
    return 1;
  }

  RecordingClient existingClient{{}};
  const worm::core::Repository<worm::core::MigrationHistory> existingRepository{
    nonOwning(existingClient),
    queryBuilder,
    "main",
  };
  existingRepository.initialize({.tables = {historyTableSnapshot()}});
  if (!existingClient.statements.empty()) {
    std::cerr << "Migration history repository recreated compatible storage.\n";
    return 1;
  }

  bool incompatibleStorageRejected = false;
  try {
    worm::core::SchemaTableSnapshot incompatible = historyTableSnapshot();
    incompatible.columns.pop_back();
    existingRepository.initialize({.tables = {std::move(incompatible)}});
  } catch (const worm::MigrationException&) {
    incompatibleStorageRejected = true;
  }
  if (!incompatibleStorageRejected) {
    std::cerr << "Migration history repository accepted incompatible storage.\n";
    return 1;
  }

  bool invalidSchemaRejected = false;
  try {
    static_cast<void>(worm::core::Repository<worm::core::MigrationHistory>{
      nonOwning(existingClient),
      queryBuilder,
      "main;drop_table",
    });
  } catch (const worm::InvalidArgException&) {
    invalidSchemaRejected = true;
  }
  if (!invalidSchemaRejected) {
    std::cerr << "Migration history repository accepted an unsafe schema identifier.\n";
    return 1;
  }

  return 0;
}
