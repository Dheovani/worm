#include <core/model/entity.hpp>
#include <core/output/hydration.hpp>
#include <core/query/filter.hpp>
#include <core/query/predicate.hpp>
#include <core/query/sql-builder.hpp>
#include <reflection/field.hpp>
#include <reflection/snapshot.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

namespace
{
  const void* volatile escapedAddress = nullptr;

  template <typename T>
  void escape(const T& value) noexcept
  {
    escapedAddress = &value;
  }

  struct BenchmarkEntity
  {
    std::int64_t id{};
    std::string name;
    bool active{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"benchmark_entities"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_benchmark_entities", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &BenchmarkEntity::id),
        worm::reflection::field("name", &BenchmarkEntity::name),
        worm::reflection::field("active", &BenchmarkEntity::active)};
    }
  };

  struct Measurement
  {
    std::string_view name;
    std::uint64_t totalNanoseconds;
    double nanosecondsPerOperation;
    std::uint64_t checksum;
  };

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

  [[nodiscard]]
  std::size_t parseIterations(int argc, char** argv)
  {
    constexpr std::size_t defaultIterations = 100'000;
    if (argc == 1) {
      return defaultIterations;
    }

    std::size_t iterations = 0;
    const std::string_view value{argv[1]};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), iterations);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || iterations == 0) {
      return 0;
    }

    return iterations;
  }
} // namespace

int main(int argc, char** argv)
{
  const std::size_t iterations = parseIterations(argc, argv);
  if (iterations == 0) {
    std::cerr << "Usage: WormCoreBenchmarks [positive-iteration-count]\n";
    return 1;
  }

  const worm::core::ResultRow row{{
    {"id", std::int64_t{42}},
    {"name", std::string{"benchmark"}},
    {"active", true},
  }};
  BenchmarkEntity snapshotEntity{42, "benchmark", true};
  const worm::core::PgBuilder builder;
  const worm::core::Source source{"benchmark_entities", "entity"};
  const std::vector<worm::core::Field> fields{
    worm::core::Field{"id", source},
    worm::core::Field{"name", source},
    worm::core::Field{"active", source},
  };

  const std::vector<Measurement> measurements{
    measure(
      "hydration",
      iterations,
      [&row] {
        const BenchmarkEntity entity = worm::core::hydrate<BenchmarkEntity>(row);
        escape(entity);
        return static_cast<std::uint64_t>(entity.id) + entity.name.size() + static_cast<std::uint64_t>(entity.active);
      }),
    measure(
      "snapshot",
      iterations,
      [&snapshotEntity] {
        const auto snapshot = worm::reflection::make_snapshot(snapshotEntity);
        escape(snapshot);
        snapshotEntity.active = !snapshotEntity.active;
        return static_cast<std::uint64_t>(worm::reflection::changed_field_count(snapshotEntity, snapshot));
      }),
    measure(
      "query_generation",
      iterations,
      [&builder, &fields, &source] {
        const worm::core::Statement statement = builder.select(
          fields,
          source,
          {},
          worm::core::Filter{worm::core::Predicate::equal("entity.id", std::int64_t{42})});
        escape(statement);
        return statement.sql.size() + statement.parameters.size();
      }),
  };

  std::cout << "benchmark,iterations,total_ns,ns_per_operation,checksum\n";
  for (const Measurement& measurement : measurements) {
    std::cout << measurement.name << ',' << iterations << ',' << measurement.totalNanoseconds << ','
              << measurement.nanosecondsPerOperation << ',' << measurement.checksum << '\n';
  }

  return 0;
}
