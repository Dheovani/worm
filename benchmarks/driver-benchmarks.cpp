#include <connection/configuration.hpp>
#include <core/model/entity.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/query-builder.hpp>
#include <core/query/sql-builder.hpp>
#include <reflection/field.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace
{
  const void* volatile escapedAddress = nullptr;

  template <typename T>
  void escape(const T& value) noexcept
  {
    escapedAddress = &value;
  }

  struct BenchmarkValue
  {
    std::int64_t id{};

    static constexpr worm::core::View view() noexcept
    {
      return worm::core::View{"benchmark_value"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &BenchmarkValue::id)};
    }
  };

  struct Measurement
  {
    std::string_view name;
    std::uint64_t totalNanoseconds;
    double nanosecondsPerOperation;
    std::uint64_t checksum;
  };

  [[nodiscard]]
  std::string environmentValue(const char* name, std::string fallback = {})
  {
    const char* value = std::getenv(name);
    return value == nullptr ? std::move(fallback) : std::string{value};
  }

  [[nodiscard]]
  std::optional<worm::connection::DatabaseType> parseDriver(std::string_view value) noexcept
  {
    const auto driver = worm::connection::databaseTypes.find(std::string{value});
    if (driver == worm::connection::databaseTypes.end()) {
      return std::nullopt;
    }
    return driver->second;
  }

  [[nodiscard]]
  std::size_t parseIterations(std::string_view value) noexcept
  {
    std::size_t iterations = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), iterations);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || iterations == 0) {
      return 0;
    }
    return iterations;
  }

  [[nodiscard]]
  worm::connection::ConnectionConfig connectionConfig(worm::connection::DatabaseType type)
  {
    using enum worm::connection::DatabaseType;

    switch (type) {
    case PostgreSQL:
      return {
        .host = environmentValue("WORM_BENCH_POSTGRES_HOST", "127.0.0.1"),
        .username = environmentValue("WORM_BENCH_POSTGRES_USERNAME", "worm"),
        .password = environmentValue("WORM_BENCH_POSTGRES_PASSWORD", "worm"),
        .dbname = environmentValue("WORM_BENCH_POSTGRES_DBNAME", "worm_contract"),
        .port = environmentValue("WORM_BENCH_POSTGRES_PORT", "5432"),
      };
    case MySQL:
      return {
        .host = environmentValue("WORM_BENCH_MYSQL_HOST", "127.0.0.1"),
        .username = environmentValue("WORM_BENCH_MYSQL_USERNAME", "worm"),
        .password = environmentValue("WORM_BENCH_MYSQL_PASSWORD", "worm"),
        .dbname = environmentValue("WORM_BENCH_MYSQL_DBNAME", "worm_contract"),
        .port = environmentValue("WORM_BENCH_MYSQL_PORT", "3306"),
      };
    case SQLite:
      return {.dbname = environmentValue("WORM_BENCH_SQLITE_DBNAME", ":memory:")};
    case MSSQL:
      return {
        .host = environmentValue("WORM_BENCH_MSSQL_HOST", "127.0.0.1"),
        .username = environmentValue("WORM_BENCH_MSSQL_USERNAME"),
        .password = environmentValue("WORM_BENCH_MSSQL_PASSWORD"),
        .dbname = environmentValue("WORM_BENCH_MSSQL_DBNAME"),
        .port = environmentValue("WORM_BENCH_MSSQL_PORT", "1433"),
      };
    }

    return {};
  }

  [[nodiscard]]
  std::unique_ptr<worm::core::SqlBuilder> sqlBuilder(worm::connection::DatabaseType type)
  {
    using enum worm::connection::DatabaseType;

    switch (type) {
    case PostgreSQL:
      return std::make_unique<worm::core::PgBuilder>();
    case MySQL:
      return std::make_unique<worm::core::MySqlBuilder>();
    case SQLite:
      return std::make_unique<worm::core::SqliteBuilder>();
    case MSSQL:
      return std::make_unique<worm::core::SqlServerBuilder>();
    }

    return nullptr;
  }

  [[nodiscard]]
  worm::core::Statement benchmarkStatement(worm::connection::DatabaseType type)
  {
    using enum worm::connection::DatabaseType;

    switch (type) {
    case PostgreSQL:
      return {"select $1::bigint as id", {std::int64_t{42}}};
    case MySQL:
      return {"select cast(? as signed) as id", {std::int64_t{42}}};
    case SQLite:
      return {"select cast(? as integer) as id", {std::int64_t{42}}};
    case MSSQL:
      return {"select cast(? as bigint) as id", {std::int64_t{42}}};
    }

    return {};
  }

  template <typename Operation>
  [[nodiscard]]
  Measurement measure(std::string_view name, std::size_t iterations, Operation&& operation)
  {
    std::uint64_t checksum = operation();
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
      checksum += operation();
    }
    const auto finish = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start);
    return {name,
      static_cast<std::uint64_t>(elapsed.count()),
      static_cast<double>(elapsed.count()) / static_cast<double>(iterations),
      checksum};
  }
} // namespace

int main(int argc, char** argv)
try {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: WormDriverBenchmarks <postgresql|mysql|sqlite|mssql> [positive-iteration-count]\n";
    return 1;
  }

  const auto type = parseDriver(argv[1]);
  const std::size_t iterations = argc == 3 ? parseIterations(argv[2]) : 100;
  if (!type.has_value() || iterations == 0) {
    std::cerr << "Driver or iteration count is invalid.\n";
    return 1;
  }
  if (!worm::connection::isDriverEnabled(*type)) {
    std::cerr << "The requested driver is not enabled in this build.\n";
    return 1;
  }

  const worm::connection::ConnectionConfig config = connectionConfig(*type);
  const Measurement connectionMeasurement = measure("connection_open_close", iterations, [&config, &type] {
    const auto client = worm::connection::makeClient(config, *type);
    escape(client);
    return static_cast<std::uint64_t>(client->type());
  });

  const std::shared_ptr<worm::connection::Client> client = worm::connection::makeClient(config, *type);
  const std::unique_ptr<worm::core::SqlBuilder> builder = sqlBuilder(*type);
  const worm::core::QueryBuilder queryBuilder{*builder};
  const worm::core::Repository<BenchmarkValue> repository{client, queryBuilder};
  const worm::core::Statement statement = benchmarkStatement(*type);
  const Measurement statementMeasurement = measure("statement_prepare_execute_hydrate", iterations, [&] {
    const auto values = repository.findAll(statement);
    escape(values);
    return values.empty() ? 0U : static_cast<std::uint64_t>(values.front()->id);
  });

  const std::string_view driverName = worm::connection::databaseTypeName(*type);
  std::cout << "driver,benchmark,iterations,total_ns,ns_per_operation,checksum\n";
  for (const Measurement& measurement : {connectionMeasurement, statementMeasurement}) {
    std::cout << driverName << ',' << measurement.name << ',' << iterations << ',' << measurement.totalNanoseconds
              << ',' << measurement.nanosecondsPerOperation << ',' << measurement.checksum << '\n';
  }
  return 0;
} catch (const std::exception& error) {
  std::cerr << "Driver benchmark failed: " << error.what() << '\n';
  return 1;
}
