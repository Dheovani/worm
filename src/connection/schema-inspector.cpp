#include <connection/schema-inspector.hpp>

#include <core/query/statement.hpp>
#include <errors/query-execution-exception.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace worm::connection
{
  namespace
  {
    [[nodiscard]]
    const core::Parameter* findValue(const core::ResultRow& row, std::string_view name) noexcept
    {
      const auto column = std::find_if(
        row.columns.begin(), row.columns.end(), [name](const auto& candidate) { return candidate.name == name; });

      return column == row.columns.end() ? nullptr : &column->value;
    }

    [[nodiscard]]
    std::string stringValue(const core::ResultRow& row, std::string_view name)
    {
      const core::Parameter* value = findValue(row, name);
      if (value == nullptr) {
        throw QueryExecutionException("Schema introspection omitted the '" + std::string{name} + "' value.");
      }

      if (!std::holds_alternative<std::string>(*value)) {
        throw QueryExecutionException("Schema introspection returned an invalid '" + std::string{name} + "' value.");
      }

      return std::get<std::string>(*value);
    }

    [[nodiscard]]
    bool boolValue(const core::ResultRow& row, std::string_view name)
    {
      const core::Parameter* value = findValue(row, name);
      if (value == nullptr) {
        throw QueryExecutionException("Schema introspection omitted the '" + std::string{name} + "' value.");
      }

      if (const auto integer = std::get_if<std::int64_t>(value)) {
        return *integer != 0;
      }

      if (const auto boolean = std::get_if<bool>(value)) {
        return *boolean;
      }

      if (const auto text = std::get_if<std::string>(value)) {
        return *text == "1" || *text == "YES" || *text == "true";
      }

      throw QueryExecutionException("Schema introspection returned an invalid '" + std::string{name} + "' flag.");
    }

    [[nodiscard]]
    core::Statement metadataStatement(DatabaseType type)
    {
      switch (type) {
      case DatabaseType::PostgreSQL:
        return core::Statement::prepare(
          "select c.table_schema as schema_name,c.table_name as table_name,c.column_name as column_name,"
          "case when c.is_nullable='YES' then 1 else 0 end as is_nullable,"
          "case when c.is_identity='YES' or c.column_default like 'nextval(%' then 1 else 0 end as is_generated,"
          "case when exists(select 1 from information_schema.table_constraints tc join "
          "information_schema.key_column_usage ku on tc.constraint_name=ku.constraint_name and "
          "tc.constraint_schema=ku.constraint_schema where tc.table_schema=c.table_schema and "
          "tc.table_name=c.table_name and ku.column_name=c.column_name and tc.constraint_type='UNIQUE' and "
          "(select count(*) from information_schema.key_column_usage ku2 where "
          "ku2.constraint_schema=tc.constraint_schema and ku2.constraint_name=tc.constraint_name and "
          "ku2.table_schema=tc.table_schema and ku2.table_name=tc.table_name)=1) "
          "then 1 else 0 end as is_unique,"
          "case when exists(select 1 from information_schema.table_constraints tc join "
          "information_schema.key_column_usage ku on tc.constraint_name=ku.constraint_name and "
          "tc.constraint_schema=ku.constraint_schema where tc.table_schema=c.table_schema and "
          "tc.table_name=c.table_name and ku.column_name=c.column_name and tc.constraint_type='PRIMARY KEY') "
          "then 1 else 0 end as is_primary_key "
          "from information_schema.columns c where c.table_schema not in ('pg_catalog','information_schema') "
          "order by c.table_schema,c.table_name,c.ordinal_position");
      case DatabaseType::MySQL:
        return core::Statement::prepare(
          "select c.table_schema as schema_name,c.table_name as table_name,c.column_name as column_name,"
          "case when c.is_nullable='YES' then 1 else 0 end as is_nullable,"
          "case when c.extra like '%auto_increment%' or c.extra like '%generated%' then 1 else 0 end as is_generated,"
          "case when exists(select 1 from information_schema.statistics s where s.table_schema=c.table_schema and "
          "s.table_name=c.table_name and s.column_name=c.column_name and s.non_unique=0 and "
          "s.index_name<>'PRIMARY' and (select count(*) from information_schema.statistics s2 where "
          "s2.table_schema=s.table_schema and s2.table_name=s.table_name and s2.index_name=s.index_name)=1) "
          "then 1 else 0 end as is_unique,"
          "case when c.column_key='PRI' then 1 else 0 end as is_primary_key "
          "from information_schema.columns c where c.table_schema=database() "
          "order by c.table_name,c.ordinal_position");
      case DatabaseType::MSSQL:
        return core::Statement::prepare(
          "select c.table_schema as schema_name,c.table_name as table_name,c.column_name as column_name,"
          "case when c.is_nullable='YES' then 1 else 0 end as is_nullable,"
          "columnproperty(object_id(quotename(c.table_schema)+'.'+quotename(c.table_name)),c.column_name,'IsIdentity') "
          "as is_generated,"
          "case when exists(select 1 from information_schema.table_constraints tc join "
          "information_schema.key_column_usage ku on tc.constraint_name=ku.constraint_name and "
          "tc.constraint_schema=ku.constraint_schema where tc.table_schema=c.table_schema and "
          "tc.table_name=c.table_name and ku.column_name=c.column_name and tc.constraint_type='UNIQUE' and "
          "(select count(*) from information_schema.key_column_usage ku2 where "
          "ku2.constraint_schema=tc.constraint_schema and ku2.constraint_name=tc.constraint_name and "
          "ku2.table_schema=tc.table_schema and ku2.table_name=tc.table_name)=1) "
          "then 1 else 0 end as is_unique,"
          "case when exists(select 1 from information_schema.table_constraints tc join "
          "information_schema.key_column_usage ku on tc.constraint_name=ku.constraint_name and "
          "tc.constraint_schema=ku.constraint_schema where tc.table_schema=c.table_schema and "
          "tc.table_name=c.table_name and ku.column_name=c.column_name and tc.constraint_type='PRIMARY KEY') "
          "then 1 else 0 end as is_primary_key "
          "from information_schema.columns c where c.table_schema not in ('sys','INFORMATION_SCHEMA') "
          "order by c.table_schema,c.table_name,c.ordinal_position");
      case DatabaseType::SQLite:
        return core::Statement::prepare(
          "select 'main' as schema_name,m.name as table_name,p.name as column_name,"
          "case when p.[notnull]=0 and p.pk=0 then 1 else 0 end as is_nullable,"
          "case when p.pk>0 and lower(p.type)='integer' then 1 else 0 end as is_generated,"
          "case when exists(select 1 from pragma_index_list(m.name) il join pragma_index_info(il.name) ii "
          "where il.[unique]=1 and ii.name=p.name and (select count(*) from pragma_index_info(il.name))=1) "
          "then 1 else 0 end as is_unique,"
          "case when p.pk>0 then 1 else 0 end as is_primary_key "
          "from sqlite_master m join pragma_table_info(m.name) p where m.type='table' and m.name not like 'sqlite_%' "
          "order by m.name,p.cid");
      }

      throw QueryExecutionException("Unsupported database type during schema introspection.");
    }
  } // namespace

  SchemaInspector::SchemaInspector(Client& client) noexcept
    : client_(client)
  {}

  core::SchemaSnapshot SchemaInspector::inspect() const
  {
    const core::ResultSet result = client_.execute(metadataStatement(client_.type()));
    core::SchemaSnapshot snapshot;

    for (const core::ResultRow& row : result.rows()) {
      const std::string schemaName = stringValue(row, "schema_name");
      const std::string tableName = stringValue(row, "table_name");
      core::SchemaTableSnapshot* table = nullptr;

      for (auto& candidate : snapshot.tables) {
        if (candidate.schema == schemaName && candidate.name == tableName) {
          table = &candidate;
          break;
        }
      }

      if (table == nullptr) {
        snapshot.tables.push_back({.schema = schemaName, .name = tableName});
        table = &snapshot.tables.back();
      }

      const std::string columnName = stringValue(row, "column_name");
      table->columns.push_back({
        .name = columnName,
        .nullable = boolValue(row, "is_nullable"),
        .generated = boolValue(row, "is_generated"),
        .unique = boolValue(row, "is_unique"),
      });

      if (boolValue(row, "is_primary_key")) {
        table->primaryKey.push_back(columnName);
      }
    }

    return snapshot;
  }
} // namespace worm::connection
