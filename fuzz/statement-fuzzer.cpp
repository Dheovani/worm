#include <core/query/sql-builder.hpp>
#include <core/query/statement.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
  [[nodiscard]]
  std::uint64_t readInteger(const std::uint8_t* data, std::size_t size) noexcept
  {
    std::uint64_t result = 0;
    const std::size_t bytes = size < sizeof(result) ? size : sizeof(result);
    for (std::size_t index = 0; index < bytes; ++index) {
      result |= static_cast<std::uint64_t>(data[index]) << (index * 8U);
    }
    return result;
  }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  const std::string input = size == 0 ? std::string{} : std::string{reinterpret_cast<const char*>(data), size};
  const auto queries = worm::core::splitStatementQueries(input);

  for (const std::string& query : queries) {
    if (query.empty()) {
      std::abort();
    }
  }

  const std::string qualifier = input.substr(0, size < 32 ? size : 32);
  static_cast<void>(worm::core::hasFilterWhere(input, qualifier));

  const std::uint64_t integer = readInteger(data, size);
  const std::string parameterText = "worm-fuzz-parameter:" + input;
  std::vector<std::pair<std::string, worm::core::Parameter>> columns{
    {"integer_value", static_cast<std::int64_t>(integer)},
    {"text_value", parameterText},
    {"boolean_value", (integer & 1U) != 0},
  };

  const worm::core::PgBuilder builder;
  const worm::core::Statement statement = builder.insert(worm::core::Source{"fuzz_records"}, columns);
  if (statement.parameters.size() != columns.size() || statement.parameters[0] != columns[0].second ||
      statement.parameters[1] != columns[1].second || statement.parameters[2] != columns[2].second ||
      statement.sql.find(parameterText) != std::string::npos) {
    std::abort();
  }

  const auto decoded = worm::core::decode<std::int64_t>(statement.parameters[0]);
  if (!std::holds_alternative<std::int64_t>(decoded) ||
      std::get<std::int64_t>(decoded) != static_cast<std::int64_t>(integer)) {
    std::abort();
  }

  static_cast<void>(worm::core::StatementHash{}(statement));
  return 0;
}
