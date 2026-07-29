#include <core/repository.hpp>

#include <connection/client.hpp>
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

  class RecordingClient final : public worm::connection::Client
  {
  public:
    explicit RecordingClient(std::vector<worm::core::ResultSet> responses)
      : responses_(std::move(responses))
    {}

    worm::core::ResultSet executeQuery(const worm::core::Statement& statement) const override
    {
      lastStatement = statement;
      statements.push_back(statement);
      if (nextResponse_ >= responses_.size()) {
        return {};
      }

      return responses_[nextResponse_++];
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
    mutable std::string sourceAlias;
    mutable bool hasFilter{false};
    mutable std::size_t updateColumnsCount{0};
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

  RecordingClient findClient{{usersResult({{7, "Ada"}})}};
  const worm::core::Repository<User> repository{findClient, queryBuilder};
  const std::optional<User> found = repository.find(std::int64_t{7});

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

  RecordingClient emptyClient{{worm::core::ResultSet{}}};
  const worm::core::Repository<User> emptyRepository{emptyClient, queryBuilder};
  if (emptyRepository.findOne({"select empty"}).has_value()) {
    std::cerr << "Repository findOne did not return nullopt for an empty result.\n";
    return 1;
  }

  RecordingClient allClient{{usersResult({{1, "Ada"}, {2, "Grace"}})}};
  const worm::core::Repository<User> allRepository{allClient, queryBuilder};
  const std::vector<User> users = allRepository.findAll({"select many"});
  if (users.size() != 2 || users[0].name != "Ada" || users[1].name != "Grace") {
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

  RecordingClient updateClient{{worm::core::ResultSet{std::uint64_t{1}}}};
  const worm::core::Repository<User> updateRepository{updateClient, queryBuilder};
  const std::uint64_t updatedRows = updateRepository.update(std::int64_t{7}, User{.id = 99, .name = "Lovelace"});

  if (updatedRows != 1 || updateClient.statements.size() != 1) {
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
  const worm::core::Repository<User> deleteRepository{deleteClient, queryBuilder};
  deleteRepository.delete_(std::int64_t{7});

  if (deleteClient.lastStatement.parameters != std::vector<worm::core::Parameter>{std::int64_t{7}} ||
      deleteClient.lastStatement.sql.find("where " + std::string{builder.sourceAlias} + ".id = ?") == std::string::npos) {
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
