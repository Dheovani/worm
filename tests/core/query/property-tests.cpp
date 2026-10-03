#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
  class DeterministicGenerator
  {
  public:
    explicit DeterministicGenerator(std::uint64_t seed) noexcept
      : state_(seed)
    {}

    [[nodiscard]]
    std::uint64_t next() noexcept
    {
      state_ ^= state_ << 13U;
      state_ ^= state_ >> 7U;
      state_ ^= state_ << 17U;
      return state_;
    }

    [[nodiscard]]
    std::string text(std::size_t maximumLength)
    {
      static constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-$";
      const std::size_t length = static_cast<std::size_t>(next() % (maximumLength + 1));
      std::string result;
      result.reserve(length);

      for (std::size_t index = 0; index < length; ++index) {
        result += alphabet[static_cast<std::size_t>(next() % alphabet.size())];
      }

      return result;
    }

  private:
    std::uint64_t state_;
  };

  [[nodiscard]]
  bool verifyStatementSplitting(DeterministicGenerator& generator)
  {
    for (std::size_t iteration = 0; iteration < 1'000; ++iteration) {
      const std::size_t statementCount = 1 + static_cast<std::size_t>(generator.next() % 8);
      std::string sql;
      std::vector<std::string> payloads;

      for (std::size_t index = 0; index < statementCount; ++index) {
        payloads.push_back(generator.text(40));
        sql += "select '" + payloads.back() + ";value' as payload /* ; ignored */;\n";
      }

      const auto statements = worm::core::splitStatementQueries(sql);
      if (statements.size() != statementCount) {
        return false;
      }

      for (std::size_t index = 0; index < statementCount; ++index) {
        if (statements[index].find(payloads[index] + ";value") == std::string::npos) {
          return false;
        }
      }
    }

    return true;
  }

  [[nodiscard]]
  bool verifySqlAndParameterSeparation(DeterministicGenerator& generator)
  {
    const worm::core::PgBuilder builder;
    const worm::core::Source source{"property_records"};

    for (std::size_t iteration = 0; iteration < 1'000; ++iteration) {
      const std::size_t columnCount = 1 + static_cast<std::size_t>(generator.next() % 12);
      std::vector<std::pair<std::string, worm::core::Parameter>> columns;
      columns.reserve(columnCount);

      for (std::size_t index = 0; index < columnCount; ++index) {
        const std::string value = "sensitive;value-" + generator.text(24);
        columns.emplace_back("column_" + std::to_string(index), value);
      }

      const worm::core::Statement statement = builder.insert(source, columns);
      if (statement.parameters.size() != columns.size()) {
        return false;
      }

      for (std::size_t index = 0; index < columns.size(); ++index) {
        const std::string placeholder = "$" + std::to_string(index + 1);
        if (statement.sql.find(placeholder) == std::string::npos ||
            statement.parameters[index] != columns[index].second ||
            statement.sql.find(std::get<std::string>(columns[index].second)) != std::string::npos) {
          return false;
        }
      }
    }

    return true;
  }

  [[nodiscard]]
  bool verifyParameterRoundTrips(DeterministicGenerator& generator)
  {
    for (std::size_t iteration = 0; iteration < 1'000; ++iteration) {
      const auto integer = static_cast<std::int64_t>(generator.next());
      const std::string text = generator.text(64);
      const bool boolean = (generator.next() & 1U) != 0;

      const auto decodedInteger = worm::core::decode<std::int64_t>(worm::core::encode(integer));
      const auto decodedText = worm::core::decode<std::string>(worm::core::encode(text));
      const auto decodedBoolean = worm::core::decode<bool>(worm::core::encode(boolean));

      if (!std::holds_alternative<std::int64_t>(decodedInteger) || std::get<std::int64_t>(decodedInteger) != integer ||
          !std::holds_alternative<std::string>(decodedText) || std::get<std::string>(decodedText) != text ||
          !std::holds_alternative<bool>(decodedBoolean) || std::get<bool>(decodedBoolean) != boolean) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]]
  bool verifyQualifiedFilters(DeterministicGenerator& generator)
  {
    for (std::size_t iteration = 0; iteration < 1'000; ++iteration) {
      const std::string suffix = std::to_string(generator.next());
      const std::string qualifier = "record_" + suffix;
      const std::string sql = "delete from records " + qualifier + " where " + qualifier + ".id = ?";

      if (!worm::core::hasFilterWhere(sql, qualifier) || worm::core::hasFilterWhere(sql, "other_" + suffix)) {
        return false;
      }
    }

    return true;
  }
} // namespace

int main()
{
  DeterministicGenerator generator{0x5eed'c0de'd15c'a11eULL};

  if (!verifyStatementSplitting(generator)) {
    std::cerr << "Property test failed for SQL statement splitting.\n";
    return 1;
  }

  if (!verifySqlAndParameterSeparation(generator)) {
    std::cerr << "Property test failed for generated SQL and parameter separation.\n";
    return 1;
  }

  if (!verifyParameterRoundTrips(generator)) {
    std::cerr << "Property test failed for parameter encode/decode round trips.\n";
    return 1;
  }

  if (!verifyQualifiedFilters(generator)) {
    std::cerr << "Property test failed for qualified mutation filters.\n";
    return 1;
  }

  return 0;
}
