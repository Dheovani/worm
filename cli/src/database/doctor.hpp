#pragma once

#include <connection/client.hpp>
#include <connection/configuration.hpp>

#include <runner.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <ostream>
#include <string>

namespace worm::cli::database
{
  enum class DoctorCheckStatus
  {
    Passed,
    Warning,
    Failed,
    Skipped
  };

  struct DoctorCheckResult
  {
    DoctorCheck check{DoctorCheck::Configuration};
    DoctorCheckStatus status{DoctorCheckStatus::Skipped};
    std::string message;
    std::string detail;
    std::chrono::nanoseconds duration{};
  };

  struct DoctorMetrics final : public ExecutionMetrics
  {
    std::size_t checksTotal{};
    std::size_t checksPassed{};
    std::size_t checksWarned{};
    std::size_t checksFailed{};
    std::size_t checksSkipped{};
    std::chrono::nanoseconds totalDuration{};

    void writeText(std::ostream& out) const override;
    void writeJson(std::ostream& out) const override;
  };

  using DoctorClientFactory =
    std::function<std::unique_ptr<connection::Client>(const connection::ConnectionConfig&, connection::DatabaseType)>;
  using DoctorDriverAvailability = std::function<bool(connection::DatabaseType)>;

  struct DoctorDependencies
  {
    DoctorClientFactory clientFactory;
    DoctorDriverAvailability driverAvailable;
  };

  [[nodiscard]]
  ExecutionReport examine(const Invocation& invocation);

  [[nodiscard]]
  ExecutionReport examine(const Invocation& invocation, const DoctorClientFactory& clientFactory);

  [[nodiscard]]
  ExecutionReport examine(const Invocation& invocation, const DoctorDependencies& dependencies);
} // namespace worm::cli::database
