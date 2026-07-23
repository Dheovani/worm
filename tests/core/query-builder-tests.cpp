#include <core/query-builder.hpp>

#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  class RecordingBuilder final : public worm::core::SqlBuilder
  {
  public:
    std::string select(
      const std::vector<worm::core::Field>& fields,
      const worm::core::Source& source,
      const std::vector<worm::core::Relation>& relations,
      const std::optional<worm::core::Filter>& filter = std::nullopt,
      const std::vector<worm::core::Ordering>& ordering = {}) const override
    {
      fieldsCount_ = fields.size();
      sourceName_ = source.name;
      relationsCount_ = relations.size();
      hasFilter_ = filter.has_value();
      orderingCount_ = ordering.size();
      return std::string{query_};
    }

    std::string insert(
      const worm::core::Source& source,
      const std::vector<std::pair<std::string, worm::core::Parameter>>& columns) const override
    {
      sourceName_ = source.name;
      insertColumnsCount_ = columns.size();
      return std::string{insertQuery_};
    }

    std::string insertFromSelect(
      const worm::core::Source& target,
      const std::vector<std::string>& targetColumns,
      const std::string& sourceQuery) const override
    {
      sourceName_ = target.name;
      targetColumnsCount_ = targetColumns.size();
      sourceQuery_ = sourceQuery;
      return std::string{insertFromSelectQuery_};
    }

    std::string insertFromSelect(
      const worm::core::Source& target,
      const std::vector<std::string>& targetColumns,
      const std::vector<worm::core::Field>& selectedFields,
      const worm::core::Source& source,
      const std::vector<worm::core::Relation>& relations,
      const std::optional<worm::core::Filter>& filter,
      const std::vector<worm::core::Ordering>& ordering) const override
    {
      sourceName_ = source.name;
      targetName_ = target.name;
      targetColumnsCount_ = targetColumns.size();
      fieldsCount_ = selectedFields.size();
      relationsCount_ = relations.size();
      hasFilter_ = filter.has_value();
      orderingCount_ = ordering.size();
      return std::string{structuredInsertFromSelectQuery_};
    }

    std::string update(
      const worm::core::Source& source,
      const std::vector<std::pair<std::string, worm::core::Parameter>>& columns,
      const std::optional<worm::core::Filter>& filter) const override
    {
      sourceName_ = source.name;
      updateColumnsCount_ = columns.size();
      hasFilter_ = filter.has_value();
      return std::string{updateQuery_};
    }

    std::string delete_(
      const worm::core::Source& source,
      const std::optional<worm::core::Filter>& filter) const override
    {
      sourceName_ = source.name;
      hasFilter_ = filter.has_value();
      return std::string{deleteQuery_};
    }

    mutable std::size_t fieldsCount_{0};
    mutable std::string_view sourceName_;
    mutable std::string_view targetName_;
    mutable std::size_t relationsCount_{0};
    mutable bool hasFilter_{false};
    mutable std::size_t orderingCount_{0};
    mutable std::size_t insertColumnsCount_{0};
    mutable std::size_t updateColumnsCount_{0};
    mutable std::size_t targetColumnsCount_{0};
    mutable std::string sourceQuery_;

  private:
    static constexpr std::string_view query_{"select delegated"};
    static constexpr std::string_view insertQuery_{"insert delegated"};
    static constexpr std::string_view insertFromSelectQuery_{"insert from select delegated"};
    static constexpr std::string_view structuredInsertFromSelectQuery_{"structured insert from select delegated"};
    static constexpr std::string_view updateQuery_{"update delegated"};
    static constexpr std::string_view deleteQuery_{"delete delegated"};
  };
} // namespace

int main()
{
  using worm::core::Expression;
  using worm::core::Field;
  using worm::core::Filter;
  using worm::core::Join;
  using worm::core::OrderDirection;
  using worm::core::Ordering;
  using worm::core::Predicate;
  using worm::core::QueryBuilder;
  using worm::core::Relation;
  using worm::core::Source;

  const RecordingBuilder sqlBuilder;
  const QueryBuilder queryBuilder{sqlBuilder};

  const Source users{"users", "u"};
  const Source orders{"orders", "o"};
  const std::vector<Field> fields{
    Field{"id", users},
    Field{"total", orders},
  };
  const std::vector<Relation> relations{
    Relation{Join::Inner, users, orders, Expression{"u.id = o.user_id", {}}},
  };

  const auto query = queryBuilder.select(
    fields,
    users,
    relations,
    Filter{Predicate::equal("u.active", true)},
    {Ordering{"o.total", OrderDirection::Descending}});

  if (query != "select delegated") {
    std::cerr << "QueryBuilder did not return the SQL produced by the concrete builder.\n";
    return 1;
  }

  if (sqlBuilder.fieldsCount_ != 2 || sqlBuilder.sourceName_ != "users" ||
      sqlBuilder.relationsCount_ != 1 || !sqlBuilder.hasFilter_ || sqlBuilder.orderingCount_ != 1) {
    std::cerr << "QueryBuilder did not delegate the query envelope to the concrete builder.\n";
    return 1;
  }

  const std::vector<std::pair<std::string, worm::core::Parameter>> insertColumns{
    {"name", std::string{"Ada"}},
    {"active", true},
  };

  if (queryBuilder.insert(users, insertColumns) != "insert delegated" ||
      sqlBuilder.sourceName_ != "users" || sqlBuilder.insertColumnsCount_ != 2) {
    std::cerr << "QueryBuilder did not delegate insert data to the concrete builder.\n";
    return 1;
  }

  const Source archivedUsers{"archived_users"};
  const std::vector<std::string> targetColumns{
    "id",
    "total",
  };
  const std::string selectQuery{"select u.id,o.total from users u"};

  const std::string rawInsertFromSelectQuery = queryBuilder.insertFromSelect(archivedUsers, targetColumns, selectQuery);
  if (rawInsertFromSelectQuery != "insert from select delegated" ||
      sqlBuilder.sourceName_ != "archived_users" || sqlBuilder.targetColumnsCount_ != 2 ||
      sqlBuilder.sourceQuery_ != selectQuery) {
    std::cerr << "QueryBuilder did not delegate raw insert-from-select data to the concrete builder.\n";
    return 1;
  }

  const std::string structuredInsertFromSelectQuery = queryBuilder.insertFromSelect(
    archivedUsers,
    targetColumns,
    fields,
    users,
    relations,
    Filter{Predicate::equal("u.active", true)},
    {Ordering{"o.total", OrderDirection::Descending}});
  if (structuredInsertFromSelectQuery != "structured insert from select delegated" ||
      sqlBuilder.targetName_ != "archived_users" || sqlBuilder.sourceName_ != "users" ||
      sqlBuilder.targetColumnsCount_ != 2 || sqlBuilder.fieldsCount_ != 2 ||
      sqlBuilder.relationsCount_ != 1 || !sqlBuilder.hasFilter_ || sqlBuilder.orderingCount_ != 1) {
    std::cerr << "QueryBuilder did not delegate structured insert-from-select data to the concrete builder.\n";
    return 1;
  }

  const std::string updateQuery = queryBuilder.update(users, insertColumns, Filter{Predicate::equal("u.active", true)});
  if (updateQuery != "update delegated" ||
      sqlBuilder.sourceName_ != "users" || sqlBuilder.updateColumnsCount_ != 2 || !sqlBuilder.hasFilter_) {
    std::cerr << "QueryBuilder did not delegate update data to the concrete builder.\n";
    return 1;
  }

  const std::string deleteQuery = queryBuilder.delete_(users, Filter{Predicate::equal("u.active", true)});
  if (deleteQuery != "delete delegated" ||
      sqlBuilder.sourceName_ != "users" || !sqlBuilder.hasFilter_) {
    std::cerr << "QueryBuilder did not delegate delete data to the concrete builder.\n";
    return 1;
  }

  return 0;
}
