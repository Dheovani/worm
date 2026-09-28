#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace worm::connection
{
  struct DatabaseVersion
  {
    std::string value;
    std::uint32_t major{};
    std::uint32_t minor{};
    std::uint32_t patch{};
  };

  enum class DatabasePermission
  {
    Select,
    Insert,
    Update,
    Delete,
    CreateTable,
    AlterTable,
    DropTable,
    CreateIndex
  };

  enum class DatabasePermissionStatus
  {
    Granted,
    Denied,
    Unknown
  };

  struct DatabasePermissionCheck
  {
    DatabasePermission permission;
    DatabasePermissionStatus status{DatabasePermissionStatus::Unknown};
    std::string detail;
  };

  using DatabasePermissions = std::vector<DatabasePermissionCheck>;

  [[nodiscard]]
  DatabaseVersion parseDatabaseVersion(std::string value);

  [[nodiscard]]
  std::string_view databasePermissionName(DatabasePermission permission) noexcept;
} // namespace worm::connection
