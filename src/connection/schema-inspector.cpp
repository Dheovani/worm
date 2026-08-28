#include <connection/schema-inspector.hpp>

#include <core/query/statement.hpp>
#include <errors/query-execution-exception.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace worm::connection
{
  namespace
  {
    [[nodiscard]]
    const core::Parameter* findValue(const core::ResultRow& row, std::string_view name) noexcept
    {
      const auto column = std::find_if(row.columns.begin(), row.columns.end(), [name](const auto& candidate) {
        return candidate.name == name;
      });

      return column == row.columns.end() ? nullptr : &column->value;
    }

    [[nodiscard]]
    std::string stringValue(const core::ResultRow& row, std::string_view name)
    {
      const core::Parameter* value = findValue(row, name);
      if (value == nullptr) {
        throw QueryExecutionException("Schema introspection omitted the '{}' value.", name);
      }

      if (!std::holds_alternative<std::string>(*value)) {
        throw QueryExecutionException("Schema introspection returned an invalid '{}' value.", name);
      }

      return std::get<std::string>(*value);
    }

    [[nodiscard]]
    bool boolValue(const core::ResultRow& row, std::string_view name)
    {
      const core::Parameter* value = findValue(row, name);
      if (value == nullptr) {
        throw QueryExecutionException("Schema introspection omitted the '{}' value.", name);
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

      throw QueryExecutionException("Schema introspection returned an invalid '{}' flag.", name);
    }

    [[nodiscard]]
    std::optional<std::size_t> sizeValue(const core::ResultRow& row, std::string_view name)
    {
      const core::Parameter* value = findValue(row, name);
      if (value == nullptr || std::holds_alternative<std::nullptr_t>(*value)) {
        return std::nullopt;
      }

      std::uint64_t parsed = 0;
      if (const auto integer = std::get_if<std::int64_t>(value)) {
        if (*integer < 0) {
          return std::nullopt;
        }
        parsed = static_cast<std::uint64_t>(*integer);
      } else if (const auto text = std::get_if<std::string>(value)) {
        try {
          parsed = std::stoull(*text);
        } catch (const std::exception&) {
          throw QueryExecutionException("Schema introspection returned an invalid '{}' value.", name);
        }
      } else {
        throw QueryExecutionException("Schema introspection returned an invalid '{}' value.", name);
      }

      if (parsed > (std::numeric_limits<std::size_t>::max)()) {
        throw QueryExecutionException("Schema introspection returned an out-of-range '{}' value.", name);
      }
      return static_cast<std::size_t>(parsed);
    }

    [[nodiscard]]
    std::string lower(std::string value)
    {
      std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
      return value;
    }

    [[nodiscard]]
    bool contains(std::string_view value, std::string_view part) noexcept
    {
      return value.find(part) != std::string_view::npos;
    }

    [[nodiscard]]
    core::ColumnTypeKind columnTypeKind(DatabaseType database, std::string_view typeName, std::string_view nativeName)
    {
      const std::string type = lower(std::string{typeName});
      const std::string native = lower(std::string{nativeName});

      if (database == DatabaseType::SQLite) {
        if (contains(native, "bool"))
          return core::ColumnTypeKind::Boolean;
        if (contains(native, "date") && !contains(native, "time"))
          return core::ColumnTypeKind::Date;
        if (contains(native, "datetime") || contains(native, "timestamp"))
          return core::ColumnTypeKind::DateTime;
        if (contains(native, "time"))
          return core::ColumnTypeKind::Time;
        if (contains(native, "int"))
          return core::ColumnTypeKind::Int64;
        if (contains(native, "char") || contains(native, "clob") || contains(native, "text"))
          return core::ColumnTypeKind::String;
        if (native.empty() || contains(native, "blob"))
          return core::ColumnTypeKind::Binary;
        if (contains(native, "real") || contains(native, "floa") || contains(native, "doub"))
          return core::ColumnTypeKind::Float64;
        if (contains(native, "numeric") || contains(native, "decimal"))
          return core::ColumnTypeKind::Decimal;
        return core::ColumnTypeKind::Unknown;
      }

      if (type == "boolean" || type == "bool" || type == "bit" ||
          (database == DatabaseType::MySQL && type == "tinyint" && contains(native, "tinyint(1)")))
        return core::ColumnTypeKind::Boolean;
      if (type == "smallint" || type == "int2" || type == "tinyint")
        return core::ColumnTypeKind::Int16;
      if (type == "integer" || type == "int" || type == "int4" || type == "mediumint" || type == "serial")
        return core::ColumnTypeKind::Int32;
      if (type == "bigint" || type == "int8" || type == "bigserial")
        return core::ColumnTypeKind::Int64;
      if (type == "real" || type == "float4")
        return core::ColumnTypeKind::Float32;
      if (type == "double" || type == "double precision" || type == "float" || type == "float8")
        return core::ColumnTypeKind::Float64;
      if (type == "decimal" || type == "numeric" || type == "money" || type == "smallmoney")
        return core::ColumnTypeKind::Decimal;
      if (contains(type, "char") || contains(type, "text") || type == "citext" || type == "enum" || type == "set" ||
          type == "xml")
        return core::ColumnTypeKind::String;
      if (contains(type, "binary") || contains(type, "blob") || type == "bytea" || type == "image")
        return core::ColumnTypeKind::Binary;
      if (type == "date")
        return core::ColumnTypeKind::Date;
      if (type.starts_with("time") && !type.starts_with("timestamp"))
        return core::ColumnTypeKind::Time;
      if (contains(type, "timestamp") || contains(type, "datetime") || type == "smalldatetime")
        return core::ColumnTypeKind::DateTime;
      if (type == "uuid" || type == "uniqueidentifier")
        return core::ColumnTypeKind::Uuid;
      if (type == "json" || type == "jsonb")
        return core::ColumnTypeKind::Json;
      return core::ColumnTypeKind::Unknown;
    }

    [[nodiscard]]
    core::ColumnType columnType(const core::ResultRow& row, DatabaseType database)
    {
      const std::string typeName = stringValue(row, "type_name");
      const std::string nativeName = stringValue(row, "native_type");
      return {
        .kind = columnTypeKind(database, typeName, nativeName),
        .nativeName = nativeName,
        .length = sizeValue(row, "type_length"),
        .precision = sizeValue(row, "type_precision"),
        .scale = sizeValue(row, "type_scale"),
        .unsignedValue = boolValue(row, "is_unsigned"),
        .withTimeZone = boolValue(row, "has_time_zone"),
      };
    }

    [[nodiscard]]
    core::Statement metadataStatement(DatabaseType type)
    {
      switch (type) {
      case DatabaseType::PostgreSQL:
        return core::Statement::prepare(
          "select c.table_schema as schema_name,c.table_name as table_name,c.column_name as column_name,"
          "case when c.is_nullable='YES' then 1 else 0 end as is_nullable,"
          "case when c.is_identity='YES' or c.is_generated='ALWAYS' or c.column_default like 'nextval(%' "
          "then 1 else 0 end as is_generated,"
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
          "then 1 else 0 end as is_primary_key,c.data_type as type_name,c.udt_name as native_type,"
          "c.character_maximum_length as type_length,c.numeric_precision as type_precision,"
          "c.numeric_scale as type_scale,0 as is_unsigned,"
          "case when c.data_type like '%with time zone' then 1 else 0 end as has_time_zone "
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
          "case when c.column_key='PRI' then 1 else 0 end as is_primary_key,c.data_type as type_name,"
          "c.column_type as native_type,c.character_maximum_length as type_length,"
          "c.numeric_precision as type_precision,c.numeric_scale as type_scale,"
          "case when c.column_type like '%unsigned%' then 1 else 0 end as is_unsigned,0 as has_time_zone "
          "from information_schema.columns c where c.table_schema=database() "
          "order by c.table_name,c.ordinal_position");
      case DatabaseType::MSSQL:
        return core::Statement::prepare(
          "select c.table_schema as schema_name,c.table_name as table_name,c.column_name as column_name,"
          "case when c.is_nullable='YES' then 1 else 0 end as is_nullable,"
          "case when columnproperty(object_id(quotename(c.table_schema)+'.'+quotename(c.table_name)),"
          "c.column_name,'IsIdentity')=1 or columnproperty(object_id(quotename(c.table_schema)+'.'+"
          "quotename(c.table_name)),c.column_name,'IsComputed')=1 then 1 else 0 end as is_generated,"
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
          "then 1 else 0 end as is_primary_key,c.data_type as type_name,c.data_type as native_type,"
          "c.character_maximum_length as type_length,c.numeric_precision as type_precision,"
          "c.numeric_scale as type_scale,0 as is_unsigned,"
          "case when c.data_type='datetimeoffset' then 1 else 0 end as has_time_zone "
          "from information_schema.columns c where c.table_schema not in ('sys','INFORMATION_SCHEMA') "
          "order by c.table_schema,c.table_name,c.ordinal_position");
      case DatabaseType::SQLite:
        return core::Statement::prepare(
          "select 'main' as schema_name,m.name as table_name,p.name as column_name,"
          "case when p.[notnull]=0 and p.pk=0 then 1 else 0 end as is_nullable,"
          "case when p.hidden<>0 or (p.pk>0 and lower(p.type)='integer') then 1 else 0 end as is_generated,"
          "case when exists(select 1 from pragma_index_list(m.name) il join pragma_index_info(il.name) ii "
          "where il.[unique]=1 and ii.name=p.name and (select count(*) from pragma_index_info(il.name))=1) "
          "then 1 else 0 end as is_unique,"
          "case when p.pk>0 then 1 else 0 end as is_primary_key,p.type as type_name,p.type as native_type,"
          "null as type_length,null as type_precision,null as type_scale,0 as is_unsigned,0 as has_time_zone "
          "from sqlite_master m join pragma_table_xinfo(m.name) p where m.type='table' and m.name not like 'sqlite_%' "
          "order by m.name,p.cid");
      }

      throw QueryExecutionException("Unsupported database type during schema introspection.");
    }
  } // namespace

  SchemaInspector::SchemaInspector(Client& client) noexcept
    : client_(&client)
  {}

  SchemaInspector::SchemaInspector(std::unique_ptr<Client> client) noexcept
    : ownedClient_(std::move(client)),
      client_(ownedClient_.get())
  {}

  core::SchemaSnapshot SchemaInspector::inspect() const
  {
    const core::ResultSet result = client_->execute(metadataStatement(client_->type()));
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
      table->columns.push_back(
        {
          .name = columnName,
          .type = columnType(row, client_->type()),
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
