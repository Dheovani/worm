#include <core/output/hydration.hpp>

#include <errors/hydration-exception.hpp>
#include <reflection/field.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace
{
  enum class Status : std::int64_t
  {
    Inactive = 0,
    Active = 1
  };

  struct User
  {
    std::int64_t id{};
    std::string name;
    bool active{};
    Status status{Status::Inactive};
    std::optional<std::string> nickname;
    std::chrono::sys_days bornAt{};
    std::string transientValue;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &User::id),
        worm::reflection::field("name", &User::name),
        worm::reflection::field("active", &User::active),
        worm::reflection::field("status", &User::status),
        worm::reflection::field("nickname", &User::nickname),
        worm::reflection::field("bornAt", &User::bornAt, {.columnName = "born_at"}),
        worm::reflection::field("transientValue", &User::transientValue, {.ignored = true})};
    }
  };

  struct Asset
  {
    std::int64_t id{};
    worm::core::Decimal amount;
    worm::core::Binary payload;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"assets"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_assets", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Asset::id),
        worm::reflection::field("amount", &Asset::amount),
        worm::reflection::field("payload", &Asset::payload)};
    }
  };
} // namespace

int main()
{
  const worm::core::ResultRow row{{
    {"id", std::int64_t{7}},
    {"name", std::string{"Ada"}},
    {"active", true},
    {"status", std::int64_t{1}},
    {"nickname", nullptr},
    {"born_at", std::string{"1815-12-10"}},
  }};

  const User user = worm::core::hydrate<User>(row);
  if (user.id != 7 || user.name != "Ada" || !user.active || user.status != Status::Active ||
      user.nickname.has_value() ||
      user.bornAt != std::chrono::sys_days{std::chrono::year{1815} / std::chrono::December / std::chrono::day{10}}) {
    std::cerr << "Hydration did not populate a reflected entity with decoded column values.\n";
    return 1;
  }

  const Asset asset = worm::core::hydrate<Asset>({{{"id", std::int64_t{9}},
    {"amount", worm::core::Decimal{"999999999999999999.0001"}},
    {"payload", worm::core::Binary{std::byte{0x00}, std::byte{0xff}}}}});
  if (asset.amount.value() != "999999999999999999.0001" || asset.payload.size() != 2 ||
      asset.payload.value()[0] != std::byte{0x00}) {
    std::cerr << "Hydration did not preserve decimal and binary values.\n";
    return 1;
  }

  bool invalidColumnFailed = false;
  try {
    (void)worm::core::hydrate<User>({{{"missing", std::int64_t{1}}}});
  } catch (const worm::HydrationException&) {
    invalidColumnFailed = true;
  }

  bool invalidTypeFailed = false;
  try {
    (void)worm::core::hydrate<User>({{
      {"id", std::string{"not an integer"}},
      {"name", std::string{"Ada"}},
      {"active", true},
      {"status", std::int64_t{1}},
      {"nickname", nullptr},
      {"born_at", std::string{"1815-12-10"}},
    }});
  } catch (const worm::HydrationException&) {
    invalidTypeFailed = true;
  }

  bool missingColumnFailed = false;
  try {
    (void)worm::core::hydrate<User>({{
      {"id", std::int64_t{7}},
      {"name", std::string{"Ada"}},
      {"active", true},
      {"status", std::int64_t{1}},
      {"nickname", nullptr},
    }});
  } catch (const worm::HydrationException&) {
    missingColumnFailed = true;
  }

  if (!invalidColumnFailed || !invalidTypeFailed || !missingColumnFailed) {
    std::cerr << "Hydration did not report invalid columns, missing columns or conversions.\n";
    return 1;
  }

  return 0;
}
