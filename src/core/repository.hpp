#pragma once

#include <connection/client.hpp>
#include <core/model/entity-metadata.hpp>
#include <core/output/hydration.hpp>
#include <core/output/result-set.hpp>
#include <core/query/expression.hpp>
#include <core/query/filter.hpp>
#include <core/query/predicate.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/source.hpp>
#include <core/query/statement.hpp>
#include <core/query/validator.hpp>
#include <errors/invalid-operation-exception.hpp>
#include <errors/mapping-exception.hpp>
#include <errors/query-execution-exception.hpp>
#include <errors/sql-build-exception.hpp>
#include <errors/worm-exception.hpp>
#include <utils/dependency-injection.hpp>
#include <utils/hash.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace worm::core
{

  template <PersistableEntity T>
  class Repository final
  {
  public:
    explicit Repository()
      : dbClient(worm::DependencyInjector<connection::Client>().get()),
        queryBuilder()
    {}

    explicit Repository(const connection::Client& dbClient, const QueryBuilder& queryBuilder) noexcept
      : dbClient(dbClient),
        queryBuilder(queryBuilder)
    {}

    template <EncodableParameter ID>
    [[nodiscard]]
    std::optional<T> find(const ID& id) const
    try {
      const std::string alias = generateEntityAlias();
      const std::string column = alias + "." + std::string{primaryKey.columnName()};
      const Statement statement = queryBuilder.selectAll(
        {T::table().name(), alias},
        {},
        Filter{Predicate::equal(column, encode(id))});

      return findOne(statement);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::optional<T> findOne(const Statement& statement) const
    try {
      if (!isOperationValid(statement, core::Operation::Select)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      const ResultSet resultSet = dbClient.executeQuery(statement);
      const std::size_t rowCount = resultSet.rowCount();

      if (rowCount == 0) {
        return std::nullopt;
      }

      if (rowCount > 1) {
        throw worm::MappingException("More than one row returned for a single-result repository query.");
      }

      return hydrate<T>(resultSet.rows().front());
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::vector<T> findAll(const Statement& statement) const
    try {
      if (!isOperationValid(statement, core::Operation::Select)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      const ResultSet resultSet = dbClient.executeQuery(statement);

      if (resultSet.empty()) {
        return std::vector<T>{};
      }

      std::vector<T> results;
      results.reserve(resultSet.rowCount());

      for (const ResultRow& row : resultSet) {
        results.push_back(hydrate<T>(row));
      }

      return results;
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    T insert(const T& entity) const
    try {
      const std::string alias = generateEntityAlias();
      const Statement statement =
        queryBuilder.insert({T::table().name(), alias}, mapFields(entity, core::Operation::Insert));
      const ResultSet resultSet = execute(statement);

      if (resultSet.rowCount() == 1) {
        return hydrate<T>(resultSet.rows().front());
      }

      if (resultSet.rowCount() > 1) {
        throw worm::MappingException("More than one row returned for a single-result insert operation.");
      }

      if (primaryKey.isGenerated()) {
        throw worm::MappingException(
          "Insert operation did not return the generated primary key required to hydrate the created entity.");
      }

      const std::optional<T> createdEntity = find(primaryKey.get(entity));
      if (!createdEntity.has_value()) {
        throw worm::MappingException("Inserted entity could not be retrieved by its primary key.");
      }

      return createdEntity.value();
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::uint64_t insert(const std::vector<T>& entities) const
    try {
      std::uint64_t affectedRows = 0;

      for (const T& entity : entities) {
        static_cast<void>(insert(entity));
        ++affectedRows;
      }

      return affectedRows;
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::uint64_t insert(const Statement& statement) const
    try {
      if (!isOperationValid(statement, core::Operation::Insert)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      const ResultSet resultSet = execute(statement);
      return resultSet.affectedRows();
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::uint64_t insertFromSelect(
      const std::vector<std::string>& targetColumns,
      const Statement& sourceStatement) const
    try {
      if (!isOperationValid(sourceStatement, core::Operation::Select)) {
        throw worm::InvalidOperationException("The provided source statement performs an invalid operation.");
      }

      const Statement statement = queryBuilder.insertFromSelect(
        {T::table().name()},
        targetColumns,
        sourceStatement);

      return insert(statement);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::uint64_t insertFromSelect(
      const std::vector<std::string>& targetColumns,
      const std::vector<Field>& selectedFields,
      const Source& source,
      const std::vector<Relation>& relations = {},
      const std::optional<Filter>& filter = std::nullopt,
      const std::vector<Ordering>& ordering = {}) const
    try {
      const Statement statement = queryBuilder.insertFromSelect(
        {T::table().name()},
        targetColumns,
        selectedFields,
        source,
        relations,
        filter,
        ordering);

      return insert(statement);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    template <EncodableParameter ID>
    [[nodiscard]]
    std::uint64_t update(const ID& id, const T& newStateEntity) const
    try {
      const std::string alias = generateEntityAlias();
      const std::string column = alias + "." + std::string{primaryKey.columnName()};
      const Statement statement = queryBuilder.update(
        {T::table().name(), alias},
        mapFields(newStateEntity, core::Operation::Update),
        Filter{Predicate::equal(column, encode(id))});

      const ResultSet resultSet = executeFiltered(statement, alias, "UPDATE");
      return resultSet.affectedRows();
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::uint64_t update(const Statement& statement) const
    try {
      if (!isOperationValid(statement, core::Operation::Update)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      const ResultSet resultSet = executeFiltered(statement, T::table().name(), "UPDATE");
      return resultSet.affectedRows();
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    template <EncodableParameter ID>
    void delete_(const ID& id) const
    try {
      const std::string alias = generateEntityAlias();
      const std::string column = alias + "." + std::string{primaryKey.columnName()};
      const Statement statement =
        queryBuilder.delete_({T::table().name(), alias}, Filter{Predicate::equal(column, encode(id))});

      delete_(statement, alias);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    void delete_(const Statement& statement) const
    {
      if (!isOperationValid(statement, core::Operation::Delete)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      delete_(statement, T::table().name());
    }

  private:
    [[nodiscard]]
    bool isOperationValid(const Statement& statement, core::Operation kind) const
    {
      using enum core::Operation;

      switch (kind) {
      case Insert:
        return core::isInsert(statement.sql);
      case Update:
        return core::isUpdate(statement.sql);
      case Delete:
        return core::isDelete(statement.sql);
      case Select:
        return core::isSelect(statement.sql);
      default:
        return false;
      }
    }

    [[nodiscard]]
    auto mapFields(const T& entity, core::Operation kind) const
      -> std::vector<std::pair<std::string, Parameter>>
    {
      const std::size_t persistentFieldsCount = core::persistent_field_count<T>;
      std::vector<std::pair<std::string, Parameter>> result{};
      result.reserve(persistentFieldsCount);

      std::apply(
        [&](const auto&... fields) {
          ([&] {
            if (fields.isGenerated() || (kind == core::Operation::Update && fields.isPrimaryKey())) {
              return;
            }

            result.emplace_back(std::string{fields.columnName()}, encode(fields.get(entity)));
          }(), ...);
        },
        worm::core::persistent_fields_of<T>());

      return result;
    }

    [[nodiscard]]
    core::ResultSet execute(const Statement& statement) const
    try {
      return dbClient.executeQuery(statement);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    core::ResultSet executeFiltered(
      const Statement& statement,
      std::string_view filterQualifier,
      std::string_view operation) const
    try {
      if (!core::hasFilterWhere(statement.sql, filterQualifier)) {
        throw worm::SqlBuildException(
          std::string{operation} + " operation's statement must have a `WHERE` filter clause.");
      }

      return execute(statement);
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    void delete_(const Statement& statement, std::string_view filterQualifier) const
    try {
      if (!isOperationValid(statement, core::Operation::Delete)) {
        throw worm::InvalidOperationException("The provided statement performs an invalid operation.");
      }

      static_cast<void>(executeFiltered(statement, filterQualifier, "DELETE"));
    } catch (const worm::WormException&) {
      throw;
    } catch (const std::exception& error) {
      throw worm::QueryExecutionException(error.what());
    }

    [[nodiscard]]
    std::string generateEntityAlias(std::optional<std::size_t> count = std::nullopt) const
    {
      const Hash hash = worm::hashCode(T::table().name());
      std::string alias = std::string{T::table().name()} + "_" + std::to_string(hash);

      if (count.has_value()) {
        alias = alias + "_" + std::to_string(count.value());
      }

      return alias;
    }

    const connection::Client& dbClient;
    const QueryBuilder queryBuilder;

    static constexpr auto primaryKey = primary_key_field_of<T>();
  };

} // namespace worm::core
