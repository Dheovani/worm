#pragma once

#include <connection/client.hpp>
#include <connection/transaction.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/query-execution-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

namespace worm::tests
{
  struct DriverContractEntity
  {
    std::string id;
    std::string label;
    std::optional<std::string> note;

    static constexpr core::Table table() noexcept
    {
      return core::Table{"worm_driver_contract"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        reflection::field("id", &DriverContractEntity::id, {.primaryKey = true}),
        reflection::field("label", &DriverContractEntity::label),
        reflection::field("note", &DriverContractEntity::note)};
    }
  };

  inline void requireContract(bool condition, const char* message)
  {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  template <typename Client, core::SqlBuilderI Builder>
  void runDriverContract(
    Client& client,
    const Builder& sqlBuilder,
    connection::DatabaseType expectedDatabaseType)
  {
    requireContract(client.type() == expectedDatabaseType, "Driver returned the wrong database type.");

    const core::QueryBuilder queryBuilder{sqlBuilder};
    const core::Repository<DriverContractEntity> repository{client, queryBuilder};

    const std::shared_ptr<DriverContractEntity> first = repository.insert({
      .id = "first",
      .label = "Ada's record",
      .note = "bound text",
    });
    const std::shared_ptr<DriverContractEntity> second = repository.insert({
      .id = "second",
      .label = "Grace",
      .note = std::nullopt,
    });

    requireContract(first != nullptr, "Driver did not return the first inserted entity.");
    requireContract(second != nullptr, "Driver did not return the second inserted entity.");
    requireContract(
      first->label == "Ada's record" && first->note == "bound text",
      "Driver did not preserve bound text parameters.");
    requireContract(
      second->label == "Grace" && !second->note.has_value(),
      "Driver did not preserve SQL NULL separately from text.");

    const std::uint64_t updatedRows = repository.update(
      std::string{"first"},
      DriverContractEntity{.id = "first", .label = "Ada Lovelace", .note = std::nullopt});

    requireContract(updatedRows == 1, "Driver did not report one affected row for UPDATE.");
    requireContract(
      first->label == "Ada Lovelace" && !first->note.has_value(),
      "Repository did not synchronize the entity after UPDATE.");

    {
      auto transaction = client.beginTransaction();
      static_cast<void>(repository.insert({
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
      auto transaction = client.beginTransaction();
      static_cast<void>(repository.insert({
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
    requireContract(
      deletionVerification.find(std::string{"second"}) == nullptr,
      "Driver did not persist DELETE.");

    bool normalizedError = false;
    try {
      static_cast<void>(repository.findAll({"SELECT * FROM worm_missing_contract_table"}));
    } catch (const QueryExecutionException&) {
      normalizedError = true;
    }

    requireContract(normalizedError, "Driver did not normalize a database error as QueryExecutionException.");
  }
} // namespace worm::tests
