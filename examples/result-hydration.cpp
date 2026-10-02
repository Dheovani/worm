#include <core/output/hydration.hpp>
#include <core/output/result-set.hpp>
#include <reflection/field.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
  enum class AccountStatus : std::int64_t
  {
    Inactive,
    Active
  };

  struct Account
  {
    std::int64_t id{};
    std::string name;
    std::optional<std::string> nickname;
    AccountStatus status{AccountStatus::Inactive};
    std::chrono::sys_days createdAt{};
    worm::core::Decimal balance;
    worm::core::Binary signature;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"accounts"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_accounts", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Account::id),
        worm::reflection::field("name", &Account::name),
        worm::reflection::field("nickname", &Account::nickname),
        worm::reflection::field("status", &Account::status),
        worm::reflection::field("createdAt", &Account::createdAt, {.columnName = "created_at"}),
        worm::reflection::field("balance", &Account::balance),
        worm::reflection::field("signature", &Account::signature)};
    }
  };

  worm::core::ResultRow row(std::int64_t id, std::optional<std::string> nickname)
  {
    return {{
      {"id", id},
      {"name", std::string{"Ada"}},
      {"nickname", nickname.has_value() ? worm::core::Parameter{*nickname} : worm::core::Parameter{nullptr}},
      {"status", std::int64_t{1}},
      {"created_at", std::string{"1815-12-10"}},
      {"balance", worm::core::Decimal{"1234567890.0123"}},
      {"signature", worm::core::Binary{std::byte{0x00}, std::byte{0xff}}},
    }};
  }
} // namespace

int main()
{
  const worm::core::ResultSet result{{row(1, std::nullopt), row(2, std::string{})}};
  const std::vector<Account> accounts = result.hydrateAll<Account>();

  for (const Account& account : accounts) {
    std::cout << account.id << ": " << account.name << ", balance=" << account.balance.value() << "\n";
  }

  const auto expectedDate =
    std::chrono::sys_days{std::chrono::year{1815} / std::chrono::December / std::chrono::day{10}};
  return !accounts[0].nickname.has_value()
      && accounts.size() == 2
      && accounts[1].nickname == std::string{}
      && accounts[0].status == AccountStatus::Active
      && accounts[0].createdAt == expectedDate
      && accounts[0].signature.size() == 2
        ? 0 : 1;
}
