#include <core/query/sql-builder.hpp>

#include <core/query/dialect.hpp>

#include <errors/sql-build-exception.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{

  namespace
  {
    [[nodiscard]]
    std::string sqlStringLiteral(std::string_view value)
    {
      std::string result{"'"};
      for (const char character : value) {
        result += character;
        if (character == '\'') {
          result += '\'';
        }
      }
      return result + "'";
    }

    std::string aggregateName(Aggregate aggregate)
    {
      using enum Aggregate;

      switch (aggregate) {
      case Count:
        return "count";
      case Sum:
        return "sum";
      case Average:
        return "avg";
      case Minimum:
        return "min";
      case Maximum:
        return "max";
      }

      throw worm::SqlBuildException("Unsupported aggregate operation.");
    }

    std::string renderSelectField(const Field& field)
    {
      if (field.name.empty()) {
        throw worm::SqlBuildException("A projection must have a field name.");
      }

      if (field.alias.has_value() && field.alias.value().empty()) {
        throw worm::SqlBuildException("A projection alias must not be empty.");
      }

      if (field.name == "*" && field.aggregate.has_value() && field.aggregate.value() != Aggregate::Count) {
        throw worm::SqlBuildException("Only COUNT can aggregate the wildcard projection.");
      }

      std::string rendered;

      if (field.aggregate.has_value()) {
        rendered += aggregateName(field.aggregate.value());
        rendered += "(";
      }

      if (field.name != "*") {
        rendered += std::string{field.source.alias.value_or(field.source.name)};
        rendered += ".";
      } else if (!field.aggregate.has_value()) {
        rendered += std::string{field.source.alias.value_or(field.source.name)};
        rendered += ".";
      }

      rendered += std::string{field.name};

      if (field.aggregate.has_value()) {
        rendered += ")";
      }

      if (field.alias.has_value()) {
        rendered += " as ";
        rendered += std::string{field.alias.value()};
      }

      return rendered;
    }

    bool hasAggregateProjection(const std::vector<worm::core::Field>& fields)
    {
      for (const auto& field : fields) {
        if (field.aggregate.has_value()) {
          return true;
        }
      }

      return false;
    }

    bool hasPlainProjection(const std::vector<worm::core::Field>& fields)
    {
      for (const auto& field : fields) {
        if (!field.aggregate.has_value()) {
          return true;
        }
      }

      return false;
    }

    std::string listSelectFields(const std::vector<worm::core::Field>& fields)
    {
      if (fields.empty()) {
        throw worm::SqlBuildException("SELECT operation must receive at least one projection.");
      }

      std::string list;

      for (std::size_t index = 0; index < fields.size(); ++index) {
        list += renderSelectField(fields[index]);

        if (index + 1 < fields.size()) {
          list += ",";
        }
      }

      return list;
    }

    std::string getJoinClause(const Join type)
    {
      using enum Join;

      switch (type) {
      case Inner:
        return "inner join";
      case Left:
        return "left join";
      case Right:
        return "right join";
      case Full:
        return "full join";
      }

      return "inner join";
    }

    std::string getOrderDirection(const OrderDirection direction)
    {
      using enum OrderDirection;

      switch (direction) {
      case Ascending:
        return "asc";
      case Descending:
        return "desc";
      }

      return "asc";
    }

    void appendParameters(std::vector<Parameter>& target, const std::vector<Parameter>& source)
    {
      target.insert(target.end(), source.begin(), source.end());
    }

    std::vector<Parameter> relationParameters(const std::vector<Relation>& relations)
    {
      std::vector<Parameter> parameters;

      for (const auto& relation : relations) {
        appendParameters(parameters, relation.condition.parameters);
      }

      return parameters;
    }

    std::vector<Parameter> columnParameters(const std::vector<std::pair<std::string, Parameter>>& columns)
    {
      std::vector<Parameter> parameters;
      parameters.reserve(columns.size());

      for (const auto& column : columns) {
        parameters.push_back(column.second);
      }

      return parameters;
    }

  } // namespace

  std::string SqlBuilder::placeholder(std::size_t) const
  {
    return "?";
  }

  std::string SqlBuilder::renderMutationSource(const Source& source) const
  {
    std::string rendered{source.name};

    if (source.alias.has_value()) {
      rendered += " ";
      rendered += source.alias.value();
    }

    return rendered;
  }

  std::string SqliteBuilder::renderMutationSource(const Source& source) const
  {
    std::string rendered{source.name};

    if (source.alias.has_value()) {
      rendered += " as ";
      rendered += source.alias.value();
    }

    return rendered;
  }

  std::string SqlBuilder::renderUpdateFrom(const Source&) const
  {
    return {};
  }

  std::string SqlBuilder::renderDeletePrefix(const Source& source) const
  {
    return "delete from " + renderMutationSource(source);
  }

  Expression SqlBuilder::renderPagination(const Pagination& pagination, std::size_t firstParameterIndex, bool) const
  {
    return {
      " limit " + placeholder(firstParameterIndex) + " offset " + placeholder(firstParameterIndex + 1),
      {
        static_cast<std::int64_t>(pagination.limit()),
        static_cast<std::int64_t>(pagination.offset()),
      },
    };
  }

  std::string SqlServerBuilder::renderMutationSource(const Source& source) const
  {
    return std::string{source.alias.value_or(source.name)};
  }

  std::string SqlServerBuilder::renderUpdateFrom(const Source& source) const
  {
    if (!source.alias.has_value()) {
      return {};
    }

    return " from " + std::string{source.name} + " " + std::string{source.alias.value()};
  }

  std::string SqlServerBuilder::renderDeletePrefix(const Source& source) const
  {
    if (!source.alias.has_value()) {
      return "delete from " + std::string{source.name};
    }

    return "delete " + std::string{source.alias.value()} + " from " + std::string{source.name} + " " +
           std::string{source.alias.value()};
  }

  Expression SqlServerBuilder::renderPagination(
    const Pagination& pagination,
    std::size_t firstParameterIndex,
    bool hasOrdering) const
  {
    if (!hasOrdering) {
      throw worm::SqlBuildException("SQL Server pagination requires an ORDER BY clause.");
    }

    return {
      " offset " + placeholder(firstParameterIndex) + " rows fetch next " + placeholder(firstParameterIndex + 1) +
        " rows only",
      {
        static_cast<std::int64_t>(pagination.offset()),
        static_cast<std::int64_t>(pagination.limit()),
      },
    };
  }

  std::string SqlBuilder::renderExpression(const Expression& expression, std::size_t firstParameterIndex) const
  {
    std::string rendered;
    rendered.reserve(expression.sql.size());

    std::size_t parameterIndex = firstParameterIndex;
    for (const char character : expression.sql) {
      if (character == '?') {
        rendered += placeholder(parameterIndex);
        ++parameterIndex;
      } else {
        rendered += character;
      }
    }

    return rendered;
  }

  std::string SqlBuilder::buildRelations(const std::vector<Relation>& relations) const
  {
    std::string list;
    std::size_t parameterIndex = 1;

    for (std::size_t index = 0; index < relations.size(); ++index) {
      const auto& rel = relations[index];

      if (index != 0) {
        list += " ";
      }

      list += getJoinClause(rel.joinType) + " ";
      list += std::string{rel.joinedSource.name} + " ";

      if (rel.joinedSource.alias.has_value()) {
        list += std::string{rel.joinedSource.alias.value()};
      }

      list += " on (";
      list += renderExpression(rel.condition, parameterIndex);
      list += ")";
      parameterIndex += rel.condition.parameters.size();
    }

    return list;
  }

  std::string SqlBuilder::renderFilter(const Filter& filter, std::size_t firstParameterIndex) const
  {
    return renderExpression(filter.expression(), firstParameterIndex);
  }

  std::string SqlBuilder::renderGrouping(const std::vector<Grouping>& grouping) const
  {
    if (grouping.empty()) {
      return {};
    }

    std::string sql = " group by ";
    for (std::size_t index = 0; index < grouping.size(); ++index) {
      const auto& group = grouping[index];

      if (group.column.empty()) {
        throw worm::SqlBuildException("GROUP BY operation must not receive an empty column.");
      }

      if (index != 0) {
        sql += ",";
      }

      sql += std::string{group.column};
    }

    return sql;
  }

  std::string SqlBuilder::renderOrdering(const std::vector<Ordering>& ordering) const
  {
    if (ordering.empty()) {
      return {};
    }

    std::string sql = " order by ";
    for (std::size_t index = 0; index < ordering.size(); ++index) {
      const auto& order = ordering[index];

      if (index != 0) {
        sql += ",";
      }

      sql += std::string{order.column};
      sql += " ";
      sql += getOrderDirection(order.direction);
    }

    return sql;
  }

  Statement SqlBuilder::selectAll(
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering,
    const std::optional<Pagination>& pagination,
    const std::vector<Grouping>& grouping,
    const std::optional<Filter>& having) const
  {
    if (having.has_value() && grouping.empty()) {
      throw worm::SqlBuildException("HAVING requires a GROUP BY clause.");
    }

    const std::string fieldsList = std::string{source.alias.value_or(source.name)} + ".*";
    const std::string sourceName = std::string{source.name} + " " + std::string{source.alias.value_or("")};
    const std::string _relations = buildRelations(relations);
    std::string sql = "select " + fieldsList + " from " + sourceName + " " + _relations;

    if (filter.has_value()) {
      std::size_t parameterIndex = 1;
      for (const auto& relation : relations) {
        parameterIndex += relation.condition.parameters.size();
      }

      sql += " where ";
      sql += renderFilter(filter.value(), parameterIndex);
    }

    sql += renderGrouping(grouping);

    std::vector<Parameter> parameters = relationParameters(relations);
    if (filter.has_value()) {
      appendParameters(parameters, filter.value().expression().parameters);
    }

    if (having.has_value()) {
      sql += " having ";
      sql += renderFilter(having.value(), parameters.size() + 1);
      appendParameters(parameters, having.value().expression().parameters);
    }

    sql += renderOrdering(ordering);

    if (pagination.has_value()) {
      const Expression renderedPagination =
        renderPagination(pagination.value(), parameters.size() + 1, !ordering.empty());
      sql += renderedPagination.sql;
      appendParameters(parameters, renderedPagination.parameters);
    }

    return {std::move(sql), std::move(parameters)};
  }

  Statement SqlBuilder::selectAll(const Source& source, const Criteria& criteria) const
  {
    return selectAll(
      source,
      criteria.relations(),
      criteria.filter(),
      criteria.ordering(),
      criteria.pagination(),
      criteria.grouping(),
      criteria.having());
  }

  Statement SqlBuilder::select(
    const std::vector<worm::core::Field>& fields,
    const Source& source,
    const std::vector<Relation>& relations,
    const std::optional<Filter>& filter,
    const std::vector<Ordering>& ordering,
    const std::optional<Pagination>& pagination,
    const std::vector<Grouping>& grouping,
    const std::optional<Filter>& having) const
  {
    if (having.has_value() && grouping.empty()) {
      throw worm::SqlBuildException("HAVING requires a GROUP BY clause.");
    }

    if (hasAggregateProjection(fields) && hasPlainProjection(fields) && grouping.empty()) {
      throw worm::SqlBuildException("Mixed aggregate and non-aggregate projections require a GROUP BY clause.");
    }

    const std::string fieldsList = listSelectFields(fields);
    const std::string sourceName = std::string{source.name} + " " + std::string{source.alias.value_or("")};
    const std::string _relations = buildRelations(relations);
    std::string sql = "select " + fieldsList + " from " + sourceName + " " + _relations;

    if (filter.has_value()) {
      std::size_t parameterIndex = 1;
      for (const auto& relation : relations) {
        parameterIndex += relation.condition.parameters.size();
      }

      sql += " where ";
      sql += renderFilter(filter.value(), parameterIndex);
    }

    sql += renderGrouping(grouping);

    std::vector<Parameter> parameters = relationParameters(relations);
    if (filter.has_value()) {
      appendParameters(parameters, filter.value().expression().parameters);
    }

    if (having.has_value()) {
      sql += " having ";
      sql += renderFilter(having.value(), parameters.size() + 1);
      appendParameters(parameters, having.value().expression().parameters);
    }

    sql += renderOrdering(ordering);

    if (pagination.has_value()) {
      const Expression renderedPagination =
        renderPagination(pagination.value(), parameters.size() + 1, !ordering.empty());
      sql += renderedPagination.sql;
      appendParameters(parameters, renderedPagination.parameters);
    }

    return {std::move(sql), std::move(parameters)};
  }

  Statement
  SqlBuilder::select(const std::vector<worm::core::Field>& fields, const Source& source, const Criteria& criteria) const
  {
    return select(
      fields,
      source,
      criteria.relations(),
      criteria.filter(),
      criteria.ordering(),
      criteria.pagination(),
      criteria.grouping(),
      criteria.having());
  }

  Statement
  SqlBuilder::insert(const Source& source, const std::vector<std::pair<std::string, Parameter>>& columns) const
  {
    if (columns.empty()) {
      throw worm::SqlBuildException("INSERT operation must receive at least one column.");
    }

    std::string fields = "(", values = "(";

    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& field = columns[index].first;

      fields += field;
      values += placeholder(index + 1);

      if (index + 1 < columns.size()) {
        fields += ",";
        values += ",";
      }
    }

    fields += ")";
    values += ")";

    return {"insert into " + std::string{source.name} + fields + " values " + values, columnParameters(columns)};
  }

  Statement SqlBuilder::insertFromSelect(
    const Source& target,
    const std::vector<std::string>& targetColumns,
    const Statement& sourceStatement) const
  {
    if (targetColumns.empty()) {
      throw worm::SqlBuildException("INSERT FROM SELECT operation must receive at least one target column.");
    }

    std::string columns = "(";

    for (std::size_t index = 0; index < targetColumns.size(); ++index) {
      const auto& column = targetColumns[index];
      columns += column;

      if (index + 1 < targetColumns.size()) {
        columns += ",";
      }
    }

    columns += ")";

    return {"insert into " + std::string{target.name} + columns + " " + sourceStatement.sql,
      sourceStatement.parameters};
  }

  Statement SqlBuilder::insertFromSelect(
    const Source& target,
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
    const Statement selectStatement =
      select(selectedFields, source, relations, filter, ordering, pagination, grouping, having);

    return insertFromSelect(target, targetColumns, selectStatement);
  }

  Statement SqlBuilder::insertFromSelect(
    const Source& target,
    const std::vector<std::string>& targetColumns,
    const std::vector<Field>& selectedFields,
    const Source& source,
    const Criteria& criteria) const
  {
    return insertFromSelect(
      target,
      targetColumns,
      selectedFields,
      source,
      criteria.relations(),
      criteria.filter(),
      criteria.ordering(),
      criteria.pagination(),
      criteria.grouping(),
      criteria.having());
  }

  Statement SqlBuilder::update(
    const Source& source,
    const std::vector<std::pair<std::string, Parameter>>& columns,
    const std::optional<Filter>& filter) const
  {
    if (columns.empty()) {
      throw worm::SqlBuildException("UPDATE operation must receive at least one column.");
    }

    std::string sql = "update " + renderMutationSource(source) + " set ";

    for (std::size_t index = 0; index < columns.size(); ++index) {
      const auto& field = columns[index].first;

      sql += field;
      sql += " = ";
      sql += placeholder(index + 1);

      if (index + 1 < columns.size()) {
        sql += ",";
      }
    }

    sql += renderUpdateFrom(source);

    if (filter.has_value()) {
      sql += " where ";
      sql += renderFilter(filter.value(), columns.size() + 1);
    }

    std::vector<Parameter> parameters = columnParameters(columns);
    if (filter.has_value()) {
      appendParameters(parameters, filter.value().expression().parameters);
    }

    return {std::move(sql), std::move(parameters)};
  }

  Statement SqlBuilder::delete_(const Source& source, const std::optional<Filter>& filter) const
  {
    std::string sql = renderDeletePrefix(source);

    if (filter.has_value()) {
      sql += " where ";
      sql += renderFilter(filter.value(), 1);
    }

    std::vector<Parameter> parameters;
    if (filter.has_value()) {
      parameters = filter.value().expression().parameters;
    }

    return {std::move(sql), std::move(parameters)};
  }

  std::vector<Statement> SqlBuilder::create(const TableMetadata& metadata) const
  {
    const Table& table = metadata.table();

    if (table.empty()) {
      throw worm::SqlBuildException("CREATE TABLE operation requires a table name.");
    }

    if (metadata.columns().empty()) {
      throw worm::SqlBuildException("CREATE TABLE operation for '{}' requires at least one column.", table.name());
    }

    const auto qualifiedTable = [&] {
      if (table.schema().empty()) {
        return quoteIdentifier(table.name());
      }
      return quoteIdentifier(table.schema().name()) + "." + quoteIdentifier(table.name());
    }();

    const auto quotedColumns = [&](std::span<const Column> columns) {
      std::string result;
      for (std::size_t index = 0; index < columns.size(); ++index) {
        if (metadata.findColumn(columns[index].columnName) == nullptr) {
          throw worm::SqlBuildException(
            "Constraint column '{}' does not exist in table '{}'.",
            columns[index].columnName,
            table.name());
        }
        if (index != 0) {
          result += ",";
        }
        result += quoteIdentifier(columns[index].columnName);
      }
      return result;
    };

    const bool inlineGeneratedPrimaryKey =
      usesInlineGeneratedPrimaryKey() && metadata.primaryKey().has_value() &&
      metadata.primaryKey()->columns().size() == 1 &&
      metadata.findColumn(metadata.primaryKey()->columns().front().columnName) != nullptr &&
      metadata.findColumn(metadata.primaryKey()->columns().front().columnName)->generated;

    std::vector<Statement> statements;
    std::vector<std::string> definedEnums;
    std::string sql = "create table " + qualifiedTable + " (";
    for (std::size_t index = 0; index < metadata.columns().size(); ++index) {
      const ColumnMetadata& column = metadata.columns()[index];

      if (column.columnName.empty()) {
        throw worm::SqlBuildException("CREATE TABLE operation for '{}' contains an unnamed column.", table.name());
      }

      if (column.type().kind == ColumnTypeKind::Unknown) {
        throw worm::SqlBuildException("Column '{}.{}' has no supported SQL type.", table.name(), column.columnName);
      }

      if (column.type().kind == ColumnTypeKind::Enum) {
        const auto definition = renderEnumDefinition(column.type());
        if (definition.has_value() && std::ranges::find(definedEnums, *definition) == definedEnums.end()) {
          definedEnums.push_back(*definition);
          statements.push_back({*definition, {}});
        }
      }

      if (index != 0) {
        sql += ",";
      }

      sql += quoteIdentifier(column.columnName) + " " + renderColumnType(column.type());
      if (column.generated) {
        sql += renderGeneratedColumn(column);
      }

      if (!column.defaultExpression.empty()) {
        if (column.generated) {
          throw worm::SqlBuildException(
            "Generated column '{}.{}' cannot also declare a default.",
            table.name(),
            column.columnName);
        }

        if (!isSafeDdlExpression(column.defaultExpression)) {
          throw worm::SqlBuildException(
            "Column '{}.{}' contains an unsafe default expression.",
            table.name(),
            column.columnName);
        }

        sql += " default " + std::string{column.defaultExpression};
      }

      if (inlineGeneratedPrimaryKey && column.columnName == metadata.primaryKey()->columns().front().columnName) {
        sql += " primary key autoincrement";
      }

      const bool isInlinePrimaryKey =
        inlineGeneratedPrimaryKey && column.columnName == metadata.primaryKey()->columns().front().columnName;
      if (!column.nullable && !isInlinePrimaryKey) {
        sql += " not null";
      }

      if (column.unique) {
        sql += " unique";
      }
    }

    if (metadata.primaryKey().has_value() && !inlineGeneratedPrimaryKey) {
      if (metadata.primaryKey()->empty()) {
        throw worm::SqlBuildException("Primary key for table '{}' must contain at least one column.", table.name());
      }

      sql += ",";
      if (!metadata.primaryKey()->name().empty()) {
        sql += "constraint " + quoteIdentifier(metadata.primaryKey()->name()) + " ";
      }

      sql += "primary key (";
      sql += quotedColumns(metadata.primaryKey()->columns()) + ")";
    }

    for (const ForeignKey& foreignKey : metadata.foreignKeys()) {
      if (foreignKey.columns().empty() || foreignKey.columns().size() != foreignKey.referencedColumns().size()) {
        throw worm::SqlBuildException(
          "Foreign key '{}' on table '{}' has incompatible column lists.",
          foreignKey.name(),
          table.name());
      }

      const Table referencedTable = foreignKey.referencedTable();
      const std::string referencedName = renderReferencedTable(referencedTable);

      sql += ",constraint " + quoteIdentifier(foreignKey.name()) + " foreign key (";
      sql += quotedColumns(foreignKey.columns()) + ") references " + referencedName + " (";
      for (std::size_t index = 0; index < foreignKey.referencedColumns().size(); ++index) {
        if (index != 0) {
          sql += ",";
        }
        sql += quoteIdentifier(foreignKey.referencedColumns()[index].columnName);
      }
      sql += ")";

      const auto appendAction = [&](Operation operation, std::string_view operationName) {
        const ReferentialAction action = foreignKey.referentialActionFor(operation);
        if (action == ReferentialAction::NoAction) {
          return;
        }
        sql += " on ";
        sql += operationName;
        switch (action) {
        case ReferentialAction::Restrict:
          sql += " restrict";
          break;
        case ReferentialAction::Cascade:
          sql += " cascade";
          break;
        case ReferentialAction::SetNull:
          sql += " set null";
          break;
        case ReferentialAction::SetDefault:
          sql += " set default";
          break;
        case ReferentialAction::NoAction:
          break;
        }
      };

      appendAction(Operation::Update, "update");
      appendAction(Operation::Delete, "delete");
    }
    sql += ")";

    statements.push_back({std::move(sql), {}});
    statements.reserve(statements.size() + metadata.indexes().size());
    for (const Index& index : metadata.indexes()) {
      if (index.columns().empty()) {
        throw worm::SqlBuildException("Index '{}' on table '{}' has no columns.", index.name(), table.name());
      }

      std::string indexSql = "create ";
      if (index.unique()) {
        indexSql += "unique ";
      }
      indexSql += "index " + renderIndexName(index, table) + " on " + renderIndexTarget(table) + " (";
      for (std::size_t columnIndex = 0; columnIndex < index.columns().size(); ++columnIndex) {
        const IndexedColumn& indexedColumn = index.columns()[columnIndex];
        if (metadata.findColumn(indexedColumn.column.columnName) == nullptr) {
          throw worm::SqlBuildException(
            "Index '{}' references missing column '{}'.",
            index.name(),
            indexedColumn.column.columnName);
        }
        if (columnIndex != 0) {
          indexSql += ",";
        }
        indexSql += quoteIdentifier(indexedColumn.column.columnName);
        indexSql += indexedColumn.order == IndexOrder::Descending ? " desc" : " asc";
      }
      indexSql += ")";
      statements.push_back({std::move(indexSql), {}});
    }

    return statements;
  }

  std::string SqlBuilder::quoteIdentifier(std::string_view identifier) const
  {
    std::string result{"\""};
    for (const char character : identifier) {
      result += character;
      if (character == '"') {
        result += character;
      }
    }
    result += '"';
    return result;
  }

  std::string SqlBuilder::renderColumnType(const ColumnType& type) const
  {
    return PostgresDialect{}.renderColumnType(type);
  }

  std::string SqlBuilder::renderGeneratedColumn(const ColumnMetadata& column) const
  {
    if (column.type().kind != ColumnTypeKind::Int16 && column.type().kind != ColumnTypeKind::Int32 &&
        column.type().kind != ColumnTypeKind::Int64) {
      throw worm::SqlBuildException(
        "Generated column '{}.{}' must use an integer type.",
        column.table().name(),
        column.columnName);
    }
    return " generated by default as identity";
  }

  std::string SqlBuilder::renderReferencedTable(const Table& table) const
  {
    if (table.schema().empty()) {
      return quoteIdentifier(table.name());
    }

    return quoteIdentifier(table.schema().name()) + "." + quoteIdentifier(table.name());
  }

  std::string SqlBuilder::renderIndexName(const Index& index, const Table&) const
  {
    return quoteIdentifier(index.name());
  }

  std::string SqlBuilder::renderIndexTarget(const Table& table) const
  {
    return renderReferencedTable(table);
  }

  bool SqlBuilder::usesInlineGeneratedPrimaryKey() const noexcept
  {
    return false;
  }

  std::optional<std::string> SqlBuilder::renderEnumDefinition(const ColumnType& type) const
  {
    if (!type.enumeration.has_value() || type.enumeration->name.empty() || type.enumeration->values.empty()) {
      throw worm::SqlBuildException("PostgreSQL native enum requires a name and at least one value.");
    }

    std::string typeName = quoteIdentifier(type.enumeration->name);
    if (!type.enumeration->schema.empty()) {
      typeName = quoteIdentifier(type.enumeration->schema) + "." + typeName;
    }

    std::string values;
    for (const std::string& value : type.enumeration->values) {
      if (!values.empty()) {
        values += ",";
      }
      values += sqlStringLiteral(value);
    }

    const std::string schemaPredicate = type.enumeration->schema.empty()
                                          ? "n.nspname=current_schema()"
                                          : "n.nspname=" + sqlStringLiteral(type.enumeration->schema);
    const std::string enumPredicate = schemaPredicate + " and t.typname=" + sqlStringLiteral(type.enumeration->name);
    const std::string existingValues =
      "array(select e.enumlabel from pg_type t join pg_namespace n on n.oid=t.typnamespace join pg_enum e on "
      "e.enumtypid=t.oid where " +
      enumPredicate + " order by e.enumsortorder)";

    return "do $$ begin if not exists(select 1 from pg_type t join pg_namespace n on n.oid=t.typnamespace where " +
           enumPredicate + ") then create type " + typeName + " as enum (" + values + "); elsif " + existingValues +
           "<>array[" + values + "]::text[] then raise exception " +
           sqlStringLiteral("Native enum " + type.enumeration->name + " already has a different definition.") +
           "; end if; end $$";
  }

  std::string PgBuilder::placeholder(std::size_t index) const
  {
    return "$" + std::to_string(index);
  }

  std::string MySqlBuilder::quoteIdentifier(std::string_view identifier) const
  {
    std::string result{"`"};
    for (const char character : identifier) {
      result += character;
      if (character == '`') {
        result += character;
      }
    }
    result += '`';
    return result;
  }

  std::string MySqlBuilder::renderColumnType(const ColumnType& type) const
  {
    return MySqlDialect{}.renderColumnType(type);
  }

  std::string MySqlBuilder::renderGeneratedColumn(const ColumnMetadata& column) const
  {
    static_cast<void>(SqlBuilder::renderGeneratedColumn(column));
    return " auto_increment";
  }

  std::optional<std::string> MySqlBuilder::renderEnumDefinition(const ColumnType&) const
  {
    return std::nullopt;
  }

  std::string SqliteBuilder::renderColumnType(const ColumnType& type) const
  {
    return SqliteDialect{}.renderColumnType(type);
  }

  std::string SqliteBuilder::renderGeneratedColumn(const ColumnMetadata& column) const
  {
    static_cast<void>(SqlBuilder::renderGeneratedColumn(column));
    return {};
  }

  std::string SqliteBuilder::renderReferencedTable(const Table& table) const
  {
    return quoteIdentifier(table.name());
  }

  std::string SqliteBuilder::renderIndexName(const Index& index, const Table& table) const
  {
    if (table.schema().empty()) {
      return quoteIdentifier(index.name());
    }

    return quoteIdentifier(table.schema().name()) + "." + quoteIdentifier(index.name());
  }

  std::string SqliteBuilder::renderIndexTarget(const Table& table) const
  {
    return quoteIdentifier(table.name());
  }

  bool SqliteBuilder::usesInlineGeneratedPrimaryKey() const noexcept
  {
    return true;
  }

  std::string SqlServerBuilder::quoteIdentifier(std::string_view identifier) const
  {
    std::string result{"["};
    for (const char character : identifier) {
      result += character;
      if (character == ']') {
        result += character;
      }
    }
    result += ']';
    return result;
  }

  std::string SqlServerBuilder::renderColumnType(const ColumnType& type) const
  {
    return SqlServerDialect{}.renderColumnType(type);
  }

  std::string SqlServerBuilder::renderGeneratedColumn(const ColumnMetadata& column) const
  {
    static_cast<void>(SqlBuilder::renderGeneratedColumn(column));
    return " identity(1,1)";
  }

} // namespace worm::core
