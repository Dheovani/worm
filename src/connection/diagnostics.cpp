#include <connection/diagnostics.hpp>

#include <cctype>
#include <charconv>
#include <string_view>

namespace worm::connection
{
  DatabaseVersion parseDatabaseVersion(std::string value)
  {
    DatabaseVersion version{.value = std::move(value)};
    std::string_view remaining = version.value;

    while (!remaining.empty() && std::isdigit(static_cast<unsigned char>(remaining.front())) == 0) {
      remaining.remove_prefix(1);
    }

    std::uint32_t* components[]{&version.major, &version.minor, &version.patch};
    for (std::uint32_t* component : components) {
      if (remaining.empty()) {
        break;
      }

      const char* begin = remaining.data();
      const char* end = begin + remaining.size();
      const auto parsed = std::from_chars(begin, end, *component);
      if (parsed.ec != std::errc{}) {
        break;
      }

      remaining.remove_prefix(static_cast<std::size_t>(parsed.ptr - begin));
      if (remaining.empty() || remaining.front() != '.') {
        break;
      }
      remaining.remove_prefix(1);
    }

    return version;
  }

  std::string_view databasePermissionName(DatabasePermission permission) noexcept
  {
    switch (permission) {
    case DatabasePermission::Select:
      return "SELECT";
    case DatabasePermission::Insert:
      return "INSERT";
    case DatabasePermission::Update:
      return "UPDATE";
    case DatabasePermission::Delete:
      return "DELETE";
    case DatabasePermission::CreateTable:
      return "CREATE TABLE";
    case DatabasePermission::AlterTable:
      return "ALTER TABLE";
    case DatabasePermission::DropTable:
      return "DROP TABLE";
    case DatabasePermission::CreateIndex:
      return "CREATE INDEX";
    }

    return "UNKNOWN";
  }
} // namespace worm::connection
