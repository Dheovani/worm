#include <core/query/query-builder.hpp>

#include <utils/dependency-injection.hpp>

namespace worm::core
{

  QueryBuilder::QueryBuilder()
    : sqlBuilder_(worm::DependencyInjector<SqlBuilder>::get())
  {}

  QueryBuilder::QueryBuilder(const SqlBuilder& sqlBuilder) noexcept
    : sqlBuilder_(sqlBuilder)
  {}

  Statement QueryBuilder::selectAll(const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering,
    const std::optional<Pagination>& pagination,
    const std::vector<Grouping>& grouping,
    const std::optional<Filter>& having) const
  {
    return sqlBuilder_.selectAll(source, relations, filter, ordering, pagination, grouping, having);
  }

  Statement QueryBuilder::selectAll(const Source& source, const Criteria& criteria) const
  {
    return sqlBuilder_.selectAll(source, criteria);
  }

  Statement QueryBuilder::select(const std::vector<worm::core::Field>& fields,
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering,
    const std::optional<Pagination>& pagination,
    const std::vector<Grouping>& grouping,
    const std::optional<Filter>& having) const
  {
    return sqlBuilder_.select(fields, source, relations, filter, ordering, pagination, grouping, having);
  }

  Statement QueryBuilder::select(
    const std::vector<worm::core::Field>& fields, const Source& source, const Criteria& criteria) const
  {
    return sqlBuilder_.select(fields, source, criteria);
  }

  Statement QueryBuilder::insert(
    const Source& source, const std::vector<std::pair<std::string, Parameter>>& columns) const
  {
    return sqlBuilder_.insert(source, columns);
  }

  Statement QueryBuilder::insertFromSelect(
    const Source& target, const std::vector<std::string>& targetColumns, const Statement& sourceStatement) const
  {
    return sqlBuilder_.insertFromSelect(target, targetColumns, sourceStatement);
  }

  Statement QueryBuilder::insertFromSelect(const Source& target,
    const std::vector<std::string>& targetColumns,
    const std::vector<Field>& selectedFields,
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering,
    const std::optional<Pagination>& pagination,
    const std::vector<Grouping>& grouping,
    const std::optional<Filter>& having) const
  {
    return sqlBuilder_.insertFromSelect(
      target, targetColumns, selectedFields, source, relations, filter, ordering, pagination, grouping, having);
  }

  Statement QueryBuilder::insertFromSelect(const Source& target,
    const std::vector<std::string>& targetColumns,
    const std::vector<Field>& selectedFields,
    const Source& source,
    const Criteria& criteria) const
  {
    return sqlBuilder_.insertFromSelect(target, targetColumns, selectedFields, source, criteria);
  }

  Statement QueryBuilder::update(const Source& source,
    const std::vector<std::pair<std::string, Parameter>>& columns,
    const std::optional<Filter>& filter) const
  {
    return sqlBuilder_.update(source, columns, filter);
  }

  Statement QueryBuilder::delete_(const Source& source, const std::optional<Filter>& filter) const
  {
    return sqlBuilder_.delete_(source, filter);
  }

  std::vector<Statement> QueryBuilder::create(const TableMetadata& table) const
  {
    return sqlBuilder_.create(table);
  }

} // namespace worm::core
