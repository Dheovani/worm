#include <core/repository.hpp>

#include <connection/client.hpp>
#include <errors/invalid-operation-exception.hpp>
#include <errors/mapping-exception.hpp>
#include <errors/sql-build-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &User::id, {.primaryKey = true}),
        worm::reflection::field("name", &User::name)};
    }
  };

  struct GeneratedUser
  {
    std::int64_t id{};
    std::string name;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &GeneratedUser::id, {.primaryKey = true, .generated = true}),
        worm::reflection::field("name", &GeneratedUser::name)};
    }
  };

  class RecordingClient final : public worm::connection::Client
  {
  public:
    explicit RecordingClient(std::vector<worm::core::ResultSet> responses)
      : responses_(std::move(responses))
    {}

    worm::core::ResultSet execute(const worm::core::Statement& statement) override
    {
      lastStatement = statement;
      statements.push_back(statement);
      if (nextResponse_ >= responses_.size()) {
        return {};
      }

      return responses_[nextResponse_++];
    }

    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

    mutable worm::core::Statement lastStatement;
    mutable std::vector<worm::core::Statement> statements;

  private:
    std::vector<worm::core::ResultSet> responses_;
    mutable std::size_t nextResponse_{0};
  };

  class RecordingBuilder final : public worm::core::SqlBuilder
  {
  public:
    worm::core::Statement selectAll(
      const worm::core::Source& source,
      const std::vector<worm::core::Relation>&,
      const std::optional<worm::core::Filter>& filter = std::nullopt,
      const std::vector<worm::core::Ordering>& = {}) const override
    {
      sourceName = source.name;
      sourceAlias = source.alias.value_or("");
      hasFilter = filter.has_value();
      return {"select all users", filter ? filter->expression().parameters : std::vector<worm::core::Parameter>{}};
    }

    worm::core::Statement insert(
      const worm::core::Source& source,
      const std::vector<std::pair<std::string, worm::core::Parameter>>& columns) const override
    {
      sourceName = source.name;
      insertColumnsCount = columns.size();

      std::string sql = "insert into " + std::string{source.name} + " (";
      std::string values = " values (";
      std::vector<worm::core::Parameter> parameters;

      for (std::size_t index = 0; index < columns.size(); ++index) {
        sql += columns[index].first;
        values += "?";
        parameters.push_back(columns[index].second);

        if (index + 1 < columns.size()) {
          sql += ",";
          values += ",";
        }
      }

      sql += ")";
      values += ")";

      return {sql + values, std::move(parameters)};
    }

    worm::core::Statement insertFromSelect(
      const worm::core::Source& target,
      const std::vector<std::string>& targetColumns,
      const worm::core::Statement& sourceStatement) const override
    {
      sourceName = target.name;
      targetColumnsCount = targetColumns.size();
      sourceQuery = sourceStatement.sql;
      return {"insert into " + std::string{target.name} + " select delegated", sourceStatement.parameters};
    }

    worm::core::Statement insertFromSelect(
      const worm::core::Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<worm::core::Field>& selectedFields,
      const worm::core::Source& source,
      const std::vector<worm::core::Relation>& relations,
      const std::optional<worm::core::Filter>& filter = std::nullopt,
      const std::vector<worm::core::Ordering>& ordering = {}) const override
    {
      sourceName = source.name;
      targetName = target.name;
      targetColumnsCount = targetColumns.size();
      selectedFieldsCount = selectedFields.size();
      relationsCount = relations.size();
      hasFilter = filter.has_value();
      orderingCount = ordering.size();

      return {"insert into " + std::string{target.name} + " structured select delegated"};
    }

    worm::core::Statement update(
      const worm::core::Source& source,
      const std::vector<std::pair<std::string, worm::core::Parameter>>& columns,
      const std::optional<worm::core::Filter>& filter = std::nullopt) const override
    {
      sourceName = source.name;
      sourceAlias = source.alias.value_or("");
      updateColumnsCount = columns.size();
      hasFilter = filter.has_value();

      const std::string qualifier{source.alias.value_or(source.name)};
      std::string sql = "update " + std::string{source.name} + " " + qualifier + " set ";
      std::vector<worm::core::Parameter> parameters;

      for (std::size_t index = 0; index < columns.size(); ++index) {
        sql += columns[index].first;
        sql += " = ?";
        parameters.push_back(columns[index].second);

        if (index + 1 < columns.size()) {
          sql += ",";
        }
      }

      sql += " where " + qualifier + ".id = ?";

      if (filter.has_value()) {
        parameters.insert(
          parameters.end(),
          filter->expression().parameters.begin(),
          filter->expression().parameters.end());
      }

      return {std::move(sql), std::move(parameters)};
    }

    worm::core::Statement delete_(
      const worm::core::Source& source,
      const std::optional<worm::core::Filter>& filter = std::nullopt) const override
    {
      sourceName = source.name;
      sourceAlias = source.alias.value_or("");
      hasFilter = filter.has_value();

      const std::string qualifier{source.alias.value_or(source.name)};
      return {
        "delete from " + std::string{source.name} + " " + qualifier + " where " + qualifier + ".id = ?",
        filter ? filter->expression().parameters : std::vector<worm::core::Parameter>{}};
    }

    mutable std::string sourceName;
    mutable std::string targetName;
    mutable std::string sourceAlias;
    mutable bool hasFilter{false};
    mutable std::size_t insertColumnsCount{0};
    mutable std::size_t updateColumnsCount{0};
    mutable std::size_t targetColumnsCount{0};
    mutable std::size_t selectedFieldsCount{0};
    mutable std::size_t relationsCount{0};
    mutable std::size_t orderingCount{0};
    mutable std::string sourceQuery;
  };

  worm::core::ResultSet usersResult(
    std::initializer_list<std::pair<std::int64_t, std::string>> users,
    std::uint64_t affectedRows = 0)
  {
    std::vector<worm::core::ResultRow> rows;

    for (const auto& [id, name] : users) {
      rows.push_back(
        {{
          {"id", id},
          {"name", name},
        }});
    }

    return worm::core::ResultSet{std::move(rows), affectedRows};
  }
} // namespace

int main()
{
  const RecordingBuilder builder;
  const worm::core::QueryBuilder queryBuilder{builder};

  worm::core::Registry sharedRegistry;
  RecordingClient findClient{{usersResult({{7, "Ada"}})}};
  const worm::core::Repository<User> repository{findClient, queryBuilder, sharedRegistry};
  const std::shared_ptr<User> found = repository.find(std::int64_t{7});

  if (!found ||
      found->id != 7 ||
      found->name != "Ada" ||
      findClient.lastStatement.sql != "select all users" ||
      findClient.lastStatement.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}} ||
      builder.sourceName != "users" ||
      builder.sourceAlias.empty() ||
      !builder.hasFilter) {
    std::cerr << "Repository find(id) did not query by primary key and hydrate one entity.\n";
    return 1;
  }

  const std::shared_ptr<User> cached = repository.find(std::int64_t{7});
  if (!cached || cached != found || cached->name != "Ada" || findClient.statements.size() != 1) {
    std::cerr << "Repository find(id) did not reuse the registered entity.\n";
    return 1;
  }

  RecordingClient sharedFindClient{{usersResult({{8, "Grace"}})}};
  const worm::core::Repository<User> sharedRepository{sharedFindClient, queryBuilder, sharedRegistry};
  const std::shared_ptr<User> sharedFound = sharedRepository.find(std::int64_t{7});
  if (!sharedFound || sharedFound != found || sharedFound->name != "Ada" || !sharedFindClient.statements.empty()) {
    std::cerr << "Repository did not reuse entities registered by another repository.\n";
    return 1;
  }

  RecordingClient emptyClient{{worm::core::ResultSet{}}};
  const worm::core::Repository<User> emptyRepository{emptyClient, queryBuilder};
  if (emptyRepository.findOne({"select empty"}) != nullptr) {
    std::cerr << "Repository findOne did not return nullopt for an empty result.\n";
    return 1;
  }

  RecordingClient allClient{{usersResult({{1, "Ada"}, {2, "Grace"}})}};
  const worm::core::Repository<User> allRepository{allClient, queryBuilder};
  const std::vector<std::shared_ptr<User>> users = allRepository.findAll({"select many"});
  if (users.size() != 2 || users[0]->name != "Ada" || users[1]->name != "Grace") {
    std::cerr << "Repository findAll did not hydrate all rows.\n";
    return 1;
  }

  bool nonUniqueFailed = false;
  try {
    RecordingClient duplicatedClient{{usersResult({{1, "Ada"}, {2, "Grace"}})}};
    const worm::core::Repository<User> duplicatedRepository{duplicatedClient, queryBuilder};
    static_cast<void>(duplicatedRepository.findOne({"select duplicated"}));
  } catch (const worm::MappingException&) {
    nonUniqueFailed = true;
  }

  if (!nonUniqueFailed) {
    std::cerr << "Repository findOne accepted a non-unique result.\n";
    return 1;
  }

  RecordingClient insertClient{{worm::core::ResultSet{std::uint64_t{1}}, usersResult({{7, "Ada"}})}};
  const worm::core::Repository<User> insertRepository{insertClient, queryBuilder};
  const std::shared_ptr<User> inserted = insertRepository.insert(User{.id = 7, .name = "Ada"});

  if (!inserted ||
      inserted->id != 7 ||
      inserted->name != "Ada" ||
      insertClient.statements.size() != 2 ||
      insertClient.statements.front().parameters !=
        std::vector<worm::core::Parameter>{std::int64_t{7}, std::string{"Ada"}} ||
      insertClient.statements.front().sql.find("insert into users") == std::string::npos ||
      builder.insertColumnsCount != 2) {
    std::cerr << "Repository insert(entity) did not map, execute, and return the created entity.\n";
    return 1;
  }

  RecordingClient generatedInsertClient{{usersResult({{9, "Grace"}}, 1)}};
  const worm::core::Repository<GeneratedUser> generatedInsertRepository{generatedInsertClient, queryBuilder};
  const std::shared_ptr<GeneratedUser> generatedInserted = generatedInsertRepository.insert(GeneratedUser{.name = "Grace"});

  if (!generatedInserted ||
      generatedInserted->id != 9 ||
      generatedInserted->name != "Grace" ||
      generatedInsertClient.statements.size() != 1 ||
      generatedInsertClient.lastStatement.parameters != std::vector<worm::core::Parameter>{std::string{"Grace"}} ||
      builder.insertColumnsCount != 1) {
    std::cerr << "Repository insert(entity) did not use returned rows for generated primary keys.\n";
    return 1;
  }

  bool missingGeneratedIdFailed = false;
  try {
    RecordingClient missingGeneratedIdClient{{worm::core::ResultSet{std::uint64_t{1}}}};
    const worm::core::Repository<GeneratedUser> missingGeneratedIdRepository{missingGeneratedIdClient, queryBuilder};
    static_cast<void>(missingGeneratedIdRepository.insert(GeneratedUser{.name = "Missing"}));
  } catch (const worm::MappingException&) {
    missingGeneratedIdFailed = true;
  }

  if (!missingGeneratedIdFailed) {
    std::cerr << "Repository insert(entity) accepted a generated primary key without a returned row.\n";
    return 1;
  }

  RecordingClient manyInsertClient{{
    worm::core::ResultSet{std::uint64_t{1}},
    usersResult({{1, "Ada"}}),
    worm::core::ResultSet{std::uint64_t{1}},
    usersResult({{2, "Grace"}})}};
  const worm::core::Repository<User> manyInsertRepository{manyInsertClient, queryBuilder};
  const std::uint64_t manyInsertedRows =
    manyInsertRepository.insert(std::vector<User>{{.id = 1, .name = "Ada"}, {.id = 2, .name = "Grace"}});

  if (manyInsertedRows != 2 || manyInsertClient.statements.size() != 4) {
    std::cerr << "Repository insert(vector) did not aggregate affected rows.\n";
    return 1;
  }

  RecordingClient statementInsertClient{{worm::core::ResultSet{std::uint64_t{3}}}};
  const worm::core::Repository<User> statementInsertRepository{statementInsertClient, queryBuilder};
  const std::uint64_t statementInsertedRows =
    statementInsertRepository.insert({"insert into users (id,name) values (?,?)", {std::int64_t{7}, std::string{"Ada"}}});

  if (statementInsertedRows != 3 || statementInsertClient.statements.size() != 1) {
    std::cerr << "Repository insert(statement) did not return affected rows.\n";
    return 1;
  }

  bool invalidInsertFailed = false;
  try {
    RecordingClient invalidInsertClient{{worm::core::ResultSet{std::uint64_t{1}}}};
    const worm::core::Repository<User> invalidInsertRepository{invalidInsertClient, queryBuilder};
    static_cast<void>(invalidInsertRepository.insert({"select * from users"}));
  } catch (const worm::InvalidOperationException&) {
    invalidInsertFailed = true;
  }

  if (!invalidInsertFailed) {
    std::cerr << "Repository insert(statement) accepted a non-insert statement.\n";
    return 1;
  }

  RecordingClient insertFromSelectClient{{worm::core::ResultSet{std::uint64_t{4}}}};
  const worm::core::Repository<User> insertFromSelectRepository{insertFromSelectClient, queryBuilder};
  const std::uint64_t insertFromSelectRows = insertFromSelectRepository.insertFromSelect(
    {"id", "name"},
    {"select id,name from archived_users where active = ?", {true}});

  if (insertFromSelectRows != 4 ||
      insertFromSelectClient.statements.size() != 1 ||
      builder.targetColumnsCount != 2 ||
      builder.sourceQuery.find("select id,name") == std::string::npos) {
    std::cerr << "Repository insertFromSelect(statement) did not execute a generated insert statement.\n";
    return 1;
  }

  bool invalidInsertFromSelectFailed = false;
  try {
    RecordingClient invalidInsertFromSelectClient{{worm::core::ResultSet{std::uint64_t{1}}}};
    const worm::core::Repository<User> invalidInsertFromSelectRepository{invalidInsertFromSelectClient, queryBuilder};
    static_cast<void>(invalidInsertFromSelectRepository.insertFromSelect({"id", "name"}, {"delete from users"}));
  } catch (const worm::InvalidOperationException&) {
    invalidInsertFromSelectFailed = true;
  }

  if (!invalidInsertFromSelectFailed) {
    std::cerr << "Repository insertFromSelect(statement) accepted a non-select source statement.\n";
    return 1;
  }

  RecordingClient updateClient{{worm::core::ResultSet{std::uint64_t{1}}}};
  const worm::core::Repository<User> updateRepository{updateClient, queryBuilder, sharedRegistry};
  const std::uint64_t updatedRows = updateRepository.update(std::int64_t{7}, User{.id = 99, .name = "Lovelace"});

  if (updatedRows != 1 ||
      updateClient.statements.size() != 1 ||
      !sharedRegistry.instances<User>().has(7) ||
      found->id != 7 ||
      found->name != "Lovelace" ||
      sharedRegistry.instances<User>().isDirty(7)) {
    std::cerr << "Repository update(id, entity) did not return affected rows from a single update query.\n";
    return 1;
  }

  const worm::core::Statement& updateStatement = updateClient.statements.front();
  if (updateStatement.parameters != std::vector<worm::core::Parameter>{std::string{"Lovelace"}, std::int64_t{7}} ||
      updateStatement.sql.find("set name = ?") == std::string::npos ||
      updateStatement.sql.find("set id = ?") != std::string::npos ||
      updateStatement.sql.find("where " + std::string{builder.sourceAlias} + ".id = ?") == std::string::npos ||
      builder.updateColumnsCount != 1) {
    std::cerr << "Repository update(id, entity) did not map update fields safely.\n";
    return 1;
  }

  RecordingClient unchangedUpdateClient{{worm::core::ResultSet{std::uint64_t{1}}}};
  const worm::core::Repository<User> unchangedUpdateRepository{unchangedUpdateClient, queryBuilder, sharedRegistry};
  const std::uint64_t unchangedUpdatedRows =
    unchangedUpdateRepository.update(std::int64_t{7}, User{.id = 7, .name = "Lovelace"});

  if (unchangedUpdatedRows != 0 || !unchangedUpdateClient.statements.empty()) {
    std::cerr << "Repository update(id, entity) executed a query without changed fields.\n";
    return 1;
  }

  RecordingClient missingUpdateClient{{worm::core::ResultSet{std::uint64_t{0}}}};
  const worm::core::Repository<User> missingUpdateRepository{missingUpdateClient, queryBuilder};
  if (missingUpdateRepository.update(std::int64_t{7}, User{.id = 7, .name = "Nobody"}) != 0 ||
      missingUpdateClient.statements.size() != 1) {
    std::cerr << "Repository update(id, entity) did not return zero for an untouched row.\n";
    return 1;
  }

  RecordingClient bulkUpdateClient{{worm::core::ResultSet{std::uint64_t{2}}}};
  const worm::core::Repository<User> bulkUpdateRepository{bulkUpdateClient, queryBuilder};
  const std::uint64_t bulkUpdatedRows =
    bulkUpdateRepository.update({"update users set name = ? where users.id = ?", {std::string{"Changed"}, std::int64_t{1}}});

  if (bulkUpdatedRows != 2 || bulkUpdateClient.statements.size() != 1) {
    std::cerr << "Repository update(statement) did not return affected rows.\n";
    return 1;
  }

  bool unsafeUpdateFailed = false;
  try {
    RecordingClient unsafeUpdateClient{{worm::core::ResultSet{std::uint64_t{1}}}};
    const worm::core::Repository<User> unsafeUpdateRepository{unsafeUpdateClient, queryBuilder};
    static_cast<void>(unsafeUpdateRepository.update({"update users set name = ? where true"}));
  } catch (const worm::SqlBuildException&) {
    unsafeUpdateFailed = true;
  }

  if (!unsafeUpdateFailed) {
    std::cerr << "Repository update(statement) accepted a statement without a qualified WHERE filter.\n";
    return 1;
  }

  RecordingClient deleteClient{{worm::core::ResultSet{}}};
  worm::core::Registry deleteRegistry;
  deleteRegistry.instances<User>().put(7, User{.id = 7, .name = "Ada"});
  const worm::core::Repository<User> deleteRepository{deleteClient, queryBuilder, deleteRegistry};
  deleteRepository.delete_(std::int64_t{7});

  if (deleteClient.lastStatement.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}} ||
      deleteClient.lastStatement.sql.find("where " + std::string{builder.sourceAlias} + ".id = ?") == std::string::npos ||
      deleteRegistry.instances<User>().has(7)) {
    std::cerr << "Repository delete_(id) did not execute a safe aliased delete statement.\n";
    return 1;
  }

  bool unsafeDeleteFailed = false;
  try {
    RecordingClient unsafeDeleteClient{{worm::core::ResultSet{}}};
    const worm::core::Repository<User> unsafeDeleteRepository{unsafeDeleteClient, queryBuilder};
    static_cast<void>(unsafeDeleteRepository.delete_(worm::core::Statement{"delete from users where true"}));
  } catch (const worm::SqlBuildException&) {
    unsafeDeleteFailed = true;
  }

  if (!unsafeDeleteFailed) {
    std::cerr << "Repository delete_(statement) accepted a statement without a qualified WHERE filter.\n";
    return 1;
  }

  return 0;
}
