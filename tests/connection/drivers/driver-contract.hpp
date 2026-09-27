#pragma once

#include <connection/client.hpp>
#include <connection/schema-inspector.hpp>
#include <connection/transaction.hpp>
#include <core/model/schema-diff.hpp>
#include <core/model/schema-metadata.hpp>
#include <core/persistence/migration-execution.hpp>
#include <core/persistence/migration-history-repository.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/migration-ddl.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/migration-exception.hpp>
#include <errors/migration-lock-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <reflection/field.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace worm::tests
{
  struct DriverContractEntity
  {
    std::string id;
    std::string label;
    std::optional<std::string> note;
    core::Decimal amount;
    core::Binary payload;

    static constexpr core::Table table() noexcept
    {
      return core::Table{"worm_driver_contract"};
    }

    static constexpr core::PrimaryKey primaryKey() noexcept
    {
      return core::PrimaryKey{"pk_worm_driver_contract", {core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{reflection::field("id", &DriverContractEntity::id),
        reflection::field("label", &DriverContractEntity::label),
        reflection::field("note", &DriverContractEntity::note),
        reflection::field("amount", &DriverContractEntity::amount),
        reflection::field("payload", &DriverContractEntity::payload)};
    }
  };

  inline void requireContract(bool condition, const char* message)
  {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  inline core::Binary contractPayload()
  {
    std::vector<std::byte> bytes(5000);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
      bytes[index] = static_cast<std::byte>(index % 251);
    }
    return core::Binary{std::move(bytes)};
  }

  template <typename Client, core::SqlBuilderI Builder>
  void runDriverContract(
    const std::shared_ptr<Client>& client,
    const Builder& sqlBuilder,
    connection::DatabaseType expectedDatabaseType)
  {
    requireContract(client->type() == expectedDatabaseType, "Driver returned the wrong database type.");

    const core::QueryBuilder queryBuilder{sqlBuilder};
    const core::Repository<DriverContractEntity> repository{client, queryBuilder};
    const core::Binary payload = contractPayload();

    const std::shared_ptr<DriverContractEntity> first = repository.insert(
      {
        .id = "first",
        .label = "Ada's record",
        .note = "bound text",
        .amount = core::Decimal{"123456789.125"},
        .payload = payload,
      });
    const std::shared_ptr<DriverContractEntity> second = repository.insert(
      {
        .id = "second",
        .label = "Grace",
        .note = std::nullopt,
        .amount = core::Decimal{"7.5"},
        .payload = core::Binary{},
      });

    requireContract(first != nullptr, "Driver did not return the first inserted entity.");
    requireContract(second != nullptr, "Driver did not return the second inserted entity.");
    requireContract(
      first->label == "Ada's record" && first->note == "bound text",
      "Driver did not preserve bound text parameters.");
    requireContract(
      second->label == "Grace" && !second->note.has_value(),
      "Driver did not preserve SQL NULL separately from text.");
    const std::string_view expectedAmount =
      expectedDatabaseType == connection::DatabaseType::SQLite ? "123456789.125" : "123456789.125000";
    requireContract(first->amount.value() == expectedAmount, "Driver did not preserve the decimal value.");
    requireContract(
      first->payload == payload && second->payload.empty(),
      "Driver did not preserve binary values and null bytes.");

    const std::uint64_t updatedRows = repository.update(
      std::string{"first"},
      DriverContractEntity{.id = "first",
        .label = "Ada Lovelace",
        .note = std::nullopt,
        .amount = first->amount,
        .payload = first->payload});

    requireContract(updatedRows == 1, "Driver did not report one affected row for UPDATE.");
    requireContract(
      first->label == "Ada Lovelace" && !first->note.has_value(),
      "Repository did not synchronize the entity after UPDATE.");

    {
      auto transaction = client->beginTransaction();
      static_cast<void>(repository.insert(
        {
          .id = "rolled-back",
          .label = "Rollback",
          .note = std::nullopt,
        }));
    }

    const core::Repository<DriverContractEntity> rollbackVerification{client, queryBuilder};
    requireContract(
      rollbackVerification.find(std::string{"rolled-back"}) == nullptr,
      "Driver did not roll back an unfinished transaction.");

    {
      auto transaction = client->beginTransaction();
      static_cast<void>(repository.insert(
        {
          .id = "committed",
          .label = "Commit",
          .note = std::nullopt,
        }));
      transaction.commit();
    }

    const core::Repository<DriverContractEntity> commitVerification{client, queryBuilder};
    requireContract(
      commitVerification.find(std::string{"committed"}) != nullptr,
      "Driver did not commit an explicit transaction.");

    repository.delete_(std::string{"second"});

    const core::Repository<DriverContractEntity> deletionVerification{client, queryBuilder};
    requireContract(deletionVerification.find(std::string{"second"}) == nullptr, "Driver did not persist DELETE.");

    bool normalizedError = false;
    try {
      static_cast<void>(repository.findAll("SELECT * FROM worm_missing_contract_table"));
    } catch (const QueryExecutionException&) {
      normalizedError = true;
    }

    requireContract(normalizedError, "Driver did not normalize a database error as QueryExecutionException.");
  }

  template <typename Client, core::SqlBuilderI Builder>
  void runMigrationLockContract(
    const std::shared_ptr<Client>& owner,
    const std::shared_ptr<Client>& competitor,
    const Builder& sqlBuilder,
    std::string schema)
  {
    const core::QueryBuilder queryBuilder{sqlBuilder};
    const core::Repository<core::MigrationHistory> ownerRepository{owner, queryBuilder, schema};
    const core::Repository<core::MigrationHistory> competitorRepository{competitor, queryBuilder, std::move(schema)};

    core::MigrationLock ownerLock = ownerRepository.acquireLock(std::chrono::milliseconds{0});
    bool contentionRejected = false;
    try {
      static_cast<void>(competitorRepository.acquireLock(std::chrono::milliseconds{0}));
    } catch (const MigrationLockException&) {
      contentionRejected = true;
    }
    requireContract(contentionRejected, "Driver allowed two connections to own the same migration lock.");

    ownerLock.release();
    core::MigrationLock competitorLock = competitorRepository.acquireLock(std::chrono::milliseconds{1000});
    requireContract(competitorLock.active(), "Driver did not make a released migration lock available again.");
  }

  template <typename Client, core::SqlBuilderI Builder>
  void runMigrationExecutionContract(
    const std::shared_ptr<Client>& client,
    const Builder& sqlBuilder,
    std::string schema)
  {
    const core::QueryBuilder queryBuilder{sqlBuilder};
    const core::Repository<core::MigrationHistory> repository{client, queryBuilder, schema};
    const connection::SchemaInspector inspector{*client};
    repository.initialize(inspector.inspect());

    const core::Table table{core::Schema{schema}, "worm_migration_contract"};
    const core::Column id{"id", table};
    const core::SchemaMetadata expected{
      core::Schema{schema},
      {
        core::TableMetadata{
          table,
          {core::ColumnMetadata{id, {.kind = core::ColumnTypeKind::Int64}}},
          core::PrimaryKey{"pk_worm_migration_contract", {id}},
        },
      },
    };
    const core::SchemaSnapshot emptySchema;
    const core::MigrationDdlPlan ddl = core::compileMigrationDdl(
      core::generateMigrationPlan(core::compareSchemas(expected, emptySchema)),
      expected,
      emptySchema,
      sqlBuilder);

    std::vector<core::MigrationStatement> statements;
    for (const core::MigrationDdlStep& step : ddl.steps()) {
      for (const core::Statement& statement : step.statements) {
        statements.push_back(
          {
            .description = step.description,
            .sql = statement.sql,
            .risk = step.risk,
          });
      }
    }

    const core::MigrationArtifact artifact = core::makeMigrationArtifact(
      "20260928000100",
      "create-migration-contract",
      std::string{sqlBuilder.databaseName()},
      std::move(statements));
    repository.addPending(artifact);
    const core::MigrationExecutionPlan plan = core::compileMigrationExecutionPlan(artifact, sqlBuilder);

    const core::MigrationArtifact editedArtifact = core::makeMigrationArtifact(
      artifact.id(),
      "edited-migration-contract",
      std::string{sqlBuilder.databaseName()},
      {
        {
          .description = "Edited migration",
          .sql = "select 1",
        },
      });
    bool editedArtifactRejected = false;
    try {
      repository.apply(core::compileMigrationExecutionPlan(editedArtifact, sqlBuilder));
    } catch (const MigrationException&) {
      editedArtifactRejected = true;
    }
    requireContract(editedArtifactRejected, "Migration executor accepted an artifact edited after registration.");

    repository.apply(plan, core::MigrationConfirmation::Ambiguous);

    const core::MigrationHistory appliedHistory = repository.load();
    const core::MigrationRecord* applied = appliedHistory.find(artifact.id());
    requireContract(
      applied != nullptr && applied->state == core::MigrationState::Applied,
      "Migration executor did not persist successful application history.");
    requireContract(
      inspector.inspect().findTable(schema, table.name()) != nullptr,
      "Migration executor did not apply the dialect-specific CREATE TABLE statement.");

    const core::Table failureTable{core::Schema{schema}, "worm_migration_failure"};
    const core::Column failureId{"id", failureTable};
    const core::TableMetadata failureMetadata{
      failureTable,
      {core::ColumnMetadata{failureId, {.kind = core::ColumnTypeKind::Int64}}},
      core::PrimaryKey{"pk_worm_migration_failure", {failureId}},
    };
    const std::string createFailureTable = sqlBuilder.create(failureMetadata).front().sql;
    const core::MigrationArtifact failingArtifact = core::makeMigrationArtifact(
      "20260928000200",
      "fail-after-create",
      std::string{sqlBuilder.databaseName()},
      {
        {
          .description = "Create failure contract table",
          .sql = createFailureTable,
        },
        {
          .description = "Attempt duplicate table creation",
          .sql = createFailureTable,
        },
      });
    repository.addPending(failingArtifact);
    const core::MigrationExecutionPlan failingPlan = core::compileMigrationExecutionPlan(failingArtifact, sqlBuilder);

    bool failureNormalized = false;
    try {
      repository.apply(failingPlan);
    } catch (const MigrationException&) {
      failureNormalized = true;
    }
    requireContract(failureNormalized, "Migration executor did not normalize a failed statement.");

    const core::MigrationHistory failedHistory = repository.load();
    const core::MigrationRecord* failed = failedHistory.find(failingArtifact.id());
    requireContract(
      failed != nullptr && failed->state == core::MigrationState::Failed && !failed->failureReason.empty(),
      "Migration executor did not persist failure history.");

    const bool failureTableExists = inspector.inspect().findTable(schema, failureTable.name()) != nullptr;
    requireContract(
      failureTableExists == failingPlan.policy().mayBePartiallyApplied(),
      "Migration failure did not follow the dialect transaction policy.");

    bool failedRetryRejected = false;
    try {
      repository.apply(failingPlan);
    } catch (const MigrationException&) {
      failedRetryRejected = true;
    }
    requireContract(failedRetryRejected, "Migration executor retried a failed migration automatically.");
  }
} // namespace worm::tests
