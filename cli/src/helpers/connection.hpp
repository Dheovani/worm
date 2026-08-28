#pragma once

#include <connection/configuration.hpp>
#include <connection/schema-inspector.hpp>

#include <parser.hpp>

namespace worm::cli
{
  [[nodiscard]]
  connection::DatabaseType databaseType(const Invocation& invocation);

  [[nodiscard]]
  std::string defaultSchema(connection::DatabaseType type);

  [[nodiscard]]
  connection::ConnectionConfig connectionConfig(const Invocation& invocation, connection::DatabaseType type);

  [[nodiscard]]
  connection::SchemaInspector schemaInspector(const Invocation& invocation);
} // namespace worm::cli
