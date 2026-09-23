#include <core/persistence/migration-history-repository.hpp>

#include <core/model/schema-metadata.hpp>
#include <core/query/filter.hpp>
#include <core/query/predicate.hpp>

#include <errors/invalid-arg-exception.hpp>
#include <errors/migration-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/worm-exception.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace worm::core
{
  namespace
  {
    inline constexpr std::string_view historyAlias = "worm_history";

    [[nodiscard]]
    bool validIdentifier(std::string_view identifier) noexcept
    {
      if (identifier.empty()) {
        return true;
      }

      const auto validFirst = [](unsigned char character) {
        return std::isalpha(character) != 0 || character == '_';
      };

      const auto validRemaining = [](unsigned char character) {
        return std::isalnum(character) != 0 || character == '_';
      };

      return validFirst(static_cast<unsigned char>(identifier.front())) &&
             std::ranges::all_of(identifier.substr(1), validRemaining);
    }

    [[nodiscard]]
    Column historyColumn(Table table, std::string_view name, bool nullable = false, bool unique = false) noexcept
    {
      return Column{
        reflection::FieldMetadata{
          .columnName = name,
          .unique = unique,
          .nullable = nullable,
        },
        table,
      };
    }

    [[nodiscard]]
    TableMetadata historyTableMetadata(std::string_view schema)
    {
      Table table{Repository<MigrationHistory>::tableName()};
      if (!schema.empty()) {
        table = Table{Schema{schema}, Repository<MigrationHistory>::tableName()};
      }
      const Column id = historyColumn(table, "id");
      const Column name = historyColumn(table, "name");
      const Column checksum = historyColumn(table, "checksum");
      const Column state = historyColumn(table, "state");
      const Column appliedAt = historyColumn(table, "applied_at", true);
      const Column rolledBackAt = historyColumn(table, "rolled_back_at", true);
      const Column failureReason = historyColumn(table, "failure_reason", true);

      return TableMetadata{
        table,
        {
          ColumnMetadata{id, {.kind = ColumnTypeKind::String, .length = 64}},
          ColumnMetadata{name, {.kind = ColumnTypeKind::String, .length = 255}},
          ColumnMetadata{checksum, {.kind = ColumnTypeKind::String, .length = 71}},
          ColumnMetadata{state, {.kind = ColumnTypeKind::String, .length = 16}},
          ColumnMetadata{appliedAt, {.kind = ColumnTypeKind::Int64}},
          ColumnMetadata{rolledBackAt, {.kind = ColumnTypeKind::Int64}},
          ColumnMetadata{failureReason, {.kind = ColumnTypeKind::String}},
        },
        PrimaryKey{"pk_worm_migrations", {id}},
      };
    }

    [[nodiscard]]
    const SchemaTableSnapshot* findHistoryTable(const SchemaSnapshot& snapshot, std::string_view schema) noexcept
    {
      if (!schema.empty()) {
        return snapshot.findTable(schema, Repository<MigrationHistory>::tableName());
      }

      const auto table = std::ranges::find_if(snapshot.tables, [](const SchemaTableSnapshot& candidate) {
        return candidate.name == Repository<MigrationHistory>::tableName();
      });

      return table == snapshot.tables.end() ? nullptr : &*table;
    }

    void validateHistoryTable(const SchemaTableSnapshot& table)
    {
      const auto requireColumn = [&](std::string_view name, ColumnTypeKind kind, bool nullable) {
        const SchemaColumnSnapshot* column = table.findColumn(name);
        if (column == nullptr || column->type.kind != kind || column->nullable != nullable) {
          throw MigrationException(
            "Worm migration history table '{}.{}' has an incompatible '{}' column.",
            table.schema,
            table.name,
            name);
        }
      };

      requireColumn("id", ColumnTypeKind::String, false);
      requireColumn("name", ColumnTypeKind::String, false);
      requireColumn("checksum", ColumnTypeKind::String, false);
      requireColumn("state", ColumnTypeKind::String, false);
      requireColumn("applied_at", ColumnTypeKind::Int64, true);
      requireColumn("rolled_back_at", ColumnTypeKind::Int64, true);
      requireColumn("failure_reason", ColumnTypeKind::String, true);

      if (table.primaryKey.size() != 1 || table.primaryKey.front() != "id") {
        throw MigrationException(
          "Worm migration history table '{}.{}' must use 'id' as its only primary key column.",
          table.schema,
          table.name);
      }
    }

    [[nodiscard]]
    const Parameter& requiredValue(const ResultRow& row, std::string_view name)
    {
      const auto column = std::ranges::find_if(
        row.columns,
        [name](const ResultColumn& candidate) {
          return candidate.name == name;
        });

      if (column == row.columns.end()) {
        throw MigrationException("Migration history row is missing required column '{}'.", name);
      }

      return column->value;
    }

    template <DecodableParameter Value>
    [[nodiscard]]
    Value decodeHistoryValue(const ResultRow& row, std::string_view name)
    {
      const DecodeResult<Value> decoded = decode<Value>(requiredValue(row, name));
      if (std::holds_alternative<DecodeError>(decoded)) {
        throw MigrationException("Migration history column '{}' contains an incompatible value.", name);
      }
      return std::get<Value>(decoded);
    }

    [[nodiscard]]
    MigrationState parseState(std::string_view state)
    {
      if (state == "pending") {
        return MigrationState::Pending;
      }
      if (state == "applied") {
        return MigrationState::Applied;
      }
      if (state == "rolled_back") {
        return MigrationState::RolledBack;
      }
      if (state == "failed") {
        return MigrationState::Failed;
      }
      throw MigrationException("Migration history contains unknown state '{}'.", state);
    }

    [[nodiscard]]
    std::int64_t epochMilliseconds(std::chrono::system_clock::time_point value) noexcept
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
    }

    [[nodiscard]]
    std::optional<std::chrono::system_clock::time_point> decodeTimestamp(const ResultRow& row, std::string_view name)
    {
      const std::optional<std::int64_t> value = decodeHistoryValue<std::optional<std::int64_t>>(row, name);
      if (!value.has_value()) {
        return std::nullopt;
      }
      return std::chrono::system_clock::time_point{std::chrono::milliseconds{*value}};
    }

    [[nodiscard]]
    MigrationRecord decodeRecord(const ResultRow& row)
    {
      return {
        .id = decodeHistoryValue<std::string>(row, "id"),
        .name = decodeHistoryValue<std::string>(row, "name"),
        .checksum = decodeHistoryValue<std::string>(row, "checksum"),
        .state = parseState(decodeHistoryValue<std::string>(row, "state")),
        .appliedAt = decodeTimestamp(row, "applied_at"),
        .rolledBackAt = decodeTimestamp(row, "rolled_back_at"),
        .failureReason = decodeHistoryValue<std::optional<std::string>>(row, "failure_reason").value_or(""),
      };
    }
  } // namespace

  Repository<MigrationHistory>::Repository(
    std::shared_ptr<connection::Client> dbClient,
    const QueryBuilder& queryBuilder,
    std::string schema)
    : dbClient_(std::move(dbClient)),
      queryBuilder_(queryBuilder),
      schema_(std::move(schema))
  {
    if (!dbClient_) {
      throw InvalidArgException("Migration history repository requires a valid client.");
    }
    if (!validIdentifier(schema_)) {
      throw InvalidArgException("Migration history schema '{}' is not a valid SQL identifier.", schema_);
    }
  }

  void Repository<MigrationHistory>::initialize(const SchemaSnapshot& schemaSnapshot) const
  {
    const SchemaTableSnapshot* existingTable = findHistoryTable(schemaSnapshot, schema_);
    if (existingTable != nullptr) {
      validateHistoryTable(*existingTable);
      return;
    }

    for (const Statement& statement : queryBuilder_.create(historyTableMetadata(schema_))) {
      static_cast<void>(execute(statement));
    }
  }

  MigrationHistory Repository<MigrationHistory>::load() const
  {
    const std::string table = qualifiedTableName();
    const ResultSet result = execute(queryBuilder_.selectAll({table, historyAlias}));
    std::vector<MigrationRecord> records;
    records.reserve(result.rowCount());
    for (const ResultRow& row : result) {
      records.push_back(decodeRecord(row));
    }
    return MigrationHistory{std::move(records)};
  }

  void Repository<MigrationHistory>::addPending(const MigrationArtifact& artifact) const
  {
    validateMigrationArtifact(artifact);
    const std::string table = qualifiedTableName();
    const ResultSet result = execute(queryBuilder_.insert(
      Source{std::string_view{table}},
      {
        {"id", artifact.id()},
        {"name", artifact.name()},
        {"checksum", artifact.checksum()},
        {"state", std::string{"pending"}},
        {"applied_at", nullptr},
        {"rolled_back_at", nullptr},
        {"failure_reason", nullptr},
      }));

    if (result.affectedRows() != 1) {
      throw MigrationException("Unable to persist pending migration '{}'.", artifact.id());
    }
  }

  void Repository<MigrationHistory>::markApplied(
    std::string_view id,
    std::chrono::system_clock::time_point appliedAt) const
  {
    updateState(
      id,
      {
        {"state", std::string{"applied"}},
        {"applied_at", epochMilliseconds(appliedAt)},
        {"rolled_back_at", nullptr},
        {"failure_reason", nullptr},
      },
      "applied");
  }

  void Repository<MigrationHistory>::markRolledBack(
    std::string_view id,
    std::chrono::system_clock::time_point rolledBackAt) const
  {
    updateState(
      id,
      {
        {"state", std::string{"rolled_back"}},
        {"rolled_back_at", epochMilliseconds(rolledBackAt)},
      },
      "rolled back");
  }

  void Repository<MigrationHistory>::markFailed(std::string_view id, std::string_view reason) const
  {
    updateState(
      id,
      {
        {"state", std::string{"failed"}},
        {"failure_reason", std::string{reason}},
      },
      "failed");
  }

  std::string Repository<MigrationHistory>::qualifiedTableName() const
  {
    if (schema_.empty()) {
      return std::string{tableName()};
    }
    return schema_ + "." + std::string{tableName()};
  }

  ResultSet Repository<MigrationHistory>::execute(const Statement& statement) const
  try {
    return dbClient_->execute(statement);
  } catch (const WormException&) {
    throw;
  } catch (const std::exception& error) {
    throw QueryExecutionException(error.what());
  }

  void Repository<MigrationHistory>::updateState(
    std::string_view id,
    std::vector<std::pair<std::string, Parameter>> fields,
    std::string_view state) const
  {
    if (id.empty()) {
      throw InvalidArgException("Migration id cannot be empty.");
    }

    const std::string table = qualifiedTableName();
    const std::string idColumn = std::string{historyAlias} + ".id";
    const Statement statement =
      queryBuilder_.update({table, historyAlias}, fields, Filter{Predicate::equal(idColumn, std::string{id})});

    if (execute(statement).affectedRows() != 1) {
      throw MigrationException("Unable to mark migration '{}' as {}.", id, state);
    }
  }
} // namespace worm::core
