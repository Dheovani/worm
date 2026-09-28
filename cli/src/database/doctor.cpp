#include "doctor.hpp"

#include <helpers/connection.hpp>
#include <utils/dependency-injection.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    constexpr std::array allChecks{DoctorCheck::Configuration,
      DoctorCheck::Driver,
      DoctorCheck::Connection,
      DoctorCheck::Version,
      DoctorCheck::Permissions};

    struct DoctorContext
    {
      std::optional<connection::DatabaseType> type;
      std::optional<connection::ConnectionConfig> config;
      std::unique_ptr<connection::Client> client;
    };

    struct VersionPolicy
    {
      std::uint32_t minimumMajor;
      std::uint32_t minimumMinor;
      std::uint32_t maximumValidatedMajor;
    };

    [[nodiscard]]
    constexpr std::string_view checkName(DoctorCheck check) noexcept
    {
      switch (check) {
      case DoctorCheck::Configuration:
        return "Configuration";
      case DoctorCheck::Driver:
        return "Driver";
      case DoctorCheck::Connection:
        return "Connection";
      case DoctorCheck::Version:
        return "Version";
      case DoctorCheck::Permissions:
        return "Permissions";
      }
      return "Unknown";
    }

    [[nodiscard]]
    constexpr std::string_view statusName(DoctorCheckStatus status) noexcept
    {
      switch (status) {
      case DoctorCheckStatus::Passed:
        return "passed";
      case DoctorCheckStatus::Warning:
        return "warning";
      case DoctorCheckStatus::Failed:
        return "failed";
      case DoctorCheckStatus::Skipped:
        return "skipped";
      }
      return "failed";
    }

    [[nodiscard]]
    constexpr std::string_view statusMarker(DoctorCheckStatus status) noexcept
    {
      switch (status) {
      case DoctorCheckStatus::Passed:
        return "[PASS]";
      case DoctorCheckStatus::Warning:
        return "[WARN]";
      case DoctorCheckStatus::Failed:
        return "[FAIL]";
      case DoctorCheckStatus::Skipped:
        return "[SKIP]";
      }
      return "[FAIL]";
    }

    [[nodiscard]]
    bool selected(const Invocation& invocation, DoctorCheck check)
    {
      return invocation.doctorChecks.empty() ||
             std::ranges::find(invocation.doctorChecks, check) != invocation.doctorChecks.end();
    }

    template <typename Callback>
    [[nodiscard]]
    DoctorCheckResult timedCheck(DoctorCheck check, Callback&& callback)
    {
      const auto started = Clock::now();
      DoctorCheckResult result = callback();
      result.check = check;
      result.duration = Clock::now() - started;
      return result;
    }

    [[nodiscard]]
    DoctorCheckResult configurationCheck(const Invocation& invocation, DoctorContext& context)
    {
      return timedCheck(DoctorCheck::Configuration, [&] {
        if (!invocation.global.driver.has_value()) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "No database driver is configured.",
          };
        }

        if (!invocation.global.database.has_value()) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "No database name or SQLite path is configured.",
          };
        }

        try {
          context.type = databaseType(invocation);
          context.config = connectionConfig(invocation, *context.type);
        } catch (const std::exception& error) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "Worm configuration is invalid.",
            .detail = error.what(),
          };
        }

        std::ostringstream detail;
        detail << "driver=" << *invocation.global.driver << ", database=" << context.config->dbname;
        if (*context.type != connection::DatabaseType::SQLite) {
          detail << ", host=" << context.config->host << ", port=" << context.config->port;
        }

        return DoctorCheckResult{
          .status = DoctorCheckStatus::Passed,
          .message = "Worm configuration is valid.",
          .detail = std::move(detail).str(),
        };
      });
    }

    [[nodiscard]]
    DoctorCheckResult driverCheck(const DoctorContext& context, const DoctorDriverAvailability& driverAvailable)
    {
      return timedCheck(DoctorCheck::Driver, [&] {
        if (!context.type.has_value()) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Skipped,
            .message = "Driver check requires a valid configuration.",
          };
        }

        const std::string driver{connection::databaseTypeName(*context.type)};
        if (!driverAvailable(*context.type)) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = driver + " driver is not enabled in this build.",
          };
        }

        return DoctorCheckResult{
          .status = DoctorCheckStatus::Passed,
          .message = driver + " driver is enabled and available.",
        };
      });
    }

    [[nodiscard]]
    DoctorCheckResult connectionCheck(DoctorContext& context, const DoctorClientFactory& clientFactory)
    {
      return timedCheck(DoctorCheck::Connection, [&] {
        try {
          context.client = clientFactory(*context.config, *context.type);
          context.client->ping();
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Passed,
            .message = "Database connection completed a read-only round trip.",
          };
        } catch (const std::exception& error) {
          context.client.reset();
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "Database connection failed.",
            .detail = error.what(),
          };
        }
      });
    }

    [[nodiscard]]
    constexpr VersionPolicy versionPolicy(connection::DatabaseType type) noexcept
    {
      switch (type) {
      case connection::DatabaseType::PostgreSQL:
        return {14, 0, 17};
      case connection::DatabaseType::MySQL:
        return {8, 0, 8};
      case connection::DatabaseType::SQLite:
        return {3, 35, 3};
      case connection::DatabaseType::MSSQL:
        return {15, 0, 16};
      }
      return {};
    }

    [[nodiscard]]
    DoctorCheckResult versionCheck(DoctorContext& context)
    {
      return timedCheck(DoctorCheck::Version, [&] {
        try {
          const connection::DatabaseVersion version = context.client->databaseVersion();
          const VersionPolicy policy = versionPolicy(*context.type);
          const bool belowMinimum = version.major < policy.minimumMajor ||
                                    (version.major == policy.minimumMajor && version.minor < policy.minimumMinor);

          if (belowMinimum) {
            return DoctorCheckResult{
              .status = DoctorCheckStatus::Failed,
              .message = "Database version " + version.value + " is unsupported.",
            };
          }

          if (version.major > policy.maximumValidatedMajor) {
            return DoctorCheckResult{
              .status = DoctorCheckStatus::Warning,
              .message = "Database version " + version.value + " is newer than the versions validated by Worm.",
            };
          }

          return DoctorCheckResult{
            .status = DoctorCheckStatus::Passed,
            .message = "Database version " + version.value + " is supported.",
          };
        } catch (const std::exception& error) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "Database version could not be determined.",
            .detail = error.what(),
          };
        }
      });
    }

    [[nodiscard]]
    std::string join(const std::vector<std::string_view>& values)
    {
      std::ostringstream text;
      for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
          text << ", ";
        }
        text << values[index];
      }
      return std::move(text).str();
    }

    [[nodiscard]]
    DoctorCheckResult permissionsCheck(DoctorContext& context)
    {
      return timedCheck(DoctorCheck::Permissions, [&] {
        try {
          const connection::DatabasePermissions permissions = context.client->databasePermissions();
          if (permissions.size() != 8) {
            return DoctorCheckResult{
              .status = DoctorCheckStatus::Failed,
              .message = "Database returned an incomplete permission diagnosis.",
            };
          }

          std::vector<std::string_view> denied;
          std::vector<std::string_view> unknown;
          for (const connection::DatabasePermissionCheck& permission : permissions) {
            if (permission.status == connection::DatabasePermissionStatus::Denied) {
              denied.push_back(connection::databasePermissionName(permission.permission));
            } else if (permission.status == connection::DatabasePermissionStatus::Unknown) {
              unknown.push_back(connection::databasePermissionName(permission.permission));
            }
          }

          if (!denied.empty()) {
            return DoctorCheckResult{
              .status = DoctorCheckStatus::Failed,
              .message = "Required database permissions are missing.",
              .detail = "Denied: " + join(denied),
            };
          }

          if (!unknown.empty()) {
            return DoctorCheckResult{
              .status = DoctorCheckStatus::Warning,
              .message = "Some database permissions could not be determined.",
              .detail = "Unknown: " + join(unknown),
            };
          }

          return DoctorCheckResult{
            .status = DoctorCheckStatus::Passed,
            .message = "Required database permissions are available.",
          };
        } catch (const std::exception& error) {
          return DoctorCheckResult{
            .status = DoctorCheckStatus::Failed,
            .message = "Database permissions could not be determined.",
            .detail = error.what(),
          };
        }
      });
    }

    void record(DoctorMetrics& metrics, const DoctorCheckResult& result)
    {
      ++metrics.checksTotal;
      switch (result.status) {
      case DoctorCheckStatus::Passed:
        ++metrics.checksPassed;
        break;
      case DoctorCheckStatus::Warning:
        ++metrics.checksWarned;
        break;
      case DoctorCheckStatus::Failed:
        ++metrics.checksFailed;
        break;
      case DoctorCheckStatus::Skipped:
        ++metrics.checksSkipped;
        break;
      }
    }

    [[nodiscard]]
    std::string sanitizedDetail(const Invocation& invocation, std::string detail)
    {
      if (!invocation.global.password.has_value() || invocation.global.password->empty()) {
        return detail;
      }

      const std::string& secret = *invocation.global.password;
      std::size_t position = 0;
      while ((position = detail.find(secret, position)) != std::string::npos) {
        detail.replace(position, secret.size(), "<redacted>");
        position += std::string_view{"<redacted>"}.size();
      }
      return detail;
    }

    [[nodiscard]]
    std::string renderResults(
      const Invocation& invocation,
      const std::vector<DoctorCheckResult>& results,
      const DoctorMetrics& metrics)
    {
      std::ostringstream out;
      if (invocation.global.format.value_or("text") == "json") {
        out << "{\"checks\":[";
        for (std::size_t index = 0; index < results.size(); ++index) {
          if (index != 0) {
            out << ',';
          }
          const DoctorCheckResult& result = results[index];
          const std::string detail = sanitizedDetail(invocation, result.detail);
          out << "{\"name\":";
          ExecutionMetrics::writeJsonString(out, checkName(result.check));
          out << ",\"status\":";
          ExecutionMetrics::writeJsonString(out, statusName(result.status));
          out << ",\"message\":";
          ExecutionMetrics::writeJsonString(out, result.message);
          if (invocation.global.verbose && !detail.empty()) {
            out << ",\"detail\":";
            ExecutionMetrics::writeJsonString(out, detail);
            out << ",\"durationMs\":" << ExecutionMetrics::milliseconds(result.duration);
          }
          out << '}';
        }
        out << "],\"metrics\":";
        metrics.writeJson(out);
        out << '}';
        return std::move(out).str();
      }

      for (const DoctorCheckResult& result : results) {
        const std::string detail = sanitizedDetail(invocation, result.detail);
        out << statusMarker(result.status) << ' ' << checkName(result.check) << ": " << result.message << '\n';
        if (invocation.global.verbose) {
          if (!detail.empty()) {
            out << "       " << detail << '\n';
          }
          out << "       Duration: " << ExecutionMetrics::milliseconds(result.duration) << " ms\n";
        }
      }
      out << '\n';
      metrics.writeText(out);
      return std::move(out).str();
    }

    void addSelected(const Invocation& invocation, std::vector<DoctorCheckResult>& results, DoctorCheckResult result)
    {
      if (selected(invocation, result.check)) {
        results.push_back(std::move(result));
      }
    }
  } // namespace

  void DoctorMetrics::writeText(std::ostream& out) const
  {
    out << "Doctor summary\n\n";
    printMetric(out, "Checks total", checksTotal);
    printMetric(out, "Passed", checksPassed);
    printMetric(out, "Warnings", checksWarned);
    printMetric(out, "Failed", checksFailed);
    printMetric(out, "Skipped", checksSkipped);
    printDuration(out, "Total", totalDuration);
  }

  void DoctorMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"checksTotal\":" << checksTotal << ",\"checksPassed\":" << checksPassed
        << ",\"checksWarned\":" << checksWarned << ",\"checksFailed\":" << checksFailed
        << ",\"checksSkipped\":" << checksSkipped << ",\"totalDurationMs\":" << milliseconds(totalDuration) << '}';
  }

  ExecutionReport examine(const Invocation& invocation)
  {
    return examine(
      invocation,
      DoctorDependencies{
        .clientFactory =
          [](const connection::ConnectionConfig& config, connection::DatabaseType type) {
            return DependencyInjector<connection::Client>::get(config, type);
          },
        .driverAvailable = connection::isDriverEnabled,
      });
  }

  ExecutionReport examine(const Invocation& invocation, const DoctorClientFactory& clientFactory)
  {
    return examine(
      invocation,
      DoctorDependencies{
        .clientFactory = clientFactory,
        .driverAvailable = connection::isDriverEnabled,
      });
  }

  ExecutionReport examine(const Invocation& invocation, const DoctorDependencies& dependencies)
  {
    const auto started = Clock::now();
    auto metrics = std::make_shared<DoctorMetrics>();
    DoctorContext context;
    std::vector<DoctorCheckResult> results;
    results.reserve(invocation.doctorChecks.empty() ? allChecks.size() : invocation.doctorChecks.size());

    DoctorCheckResult configuration = configurationCheck(invocation, context);
    addSelected(invocation, results, configuration);

    DoctorCheckResult driver{
      .check = DoctorCheck::Driver,
      .status = DoctorCheckStatus::Skipped,
      .message = "Driver check requires a valid configuration.",
    };

    const bool driverNeeded =
      selected(invocation, DoctorCheck::Driver) || selected(invocation, DoctorCheck::Connection) ||
      selected(invocation, DoctorCheck::Version) || selected(invocation, DoctorCheck::Permissions);

    if (configuration.status == DoctorCheckStatus::Passed && driverNeeded) {
      driver = driverCheck(context, dependencies.driverAvailable);
    }
    addSelected(invocation, results, driver);

    DoctorCheckResult connection{
      .check = DoctorCheck::Connection,
      .status = DoctorCheckStatus::Skipped,
      .message = "Connection check requires valid configuration and an available driver.",
    };

    const bool connectionNeeded = selected(invocation, DoctorCheck::Connection) ||
                                  selected(invocation, DoctorCheck::Version) ||
                                  selected(invocation, DoctorCheck::Permissions);

    if (driver.status == DoctorCheckStatus::Passed && connectionNeeded) {
      connection = connectionCheck(context, dependencies.clientFactory);
    }
    addSelected(invocation, results, connection);

    DoctorCheckResult version{
      .check = DoctorCheck::Version,
      .status = DoctorCheckStatus::Skipped,
      .message = "Version check requires a working database connection.",
    };

    if (connection.status == DoctorCheckStatus::Passed && selected(invocation, DoctorCheck::Version)) {
      version = versionCheck(context);
    }
    addSelected(invocation, results, version);

    DoctorCheckResult permissions{
      .check = DoctorCheck::Permissions,
      .status = DoctorCheckStatus::Skipped,
      .message = "Permissions check requires a working database connection.",
    };

    if (connection.status == DoctorCheckStatus::Passed && selected(invocation, DoctorCheck::Permissions)) {
      permissions = permissionsCheck(context);
    }
    addSelected(invocation, results, permissions);

    metrics->totalDuration = Clock::now() - started;
    for (const DoctorCheckResult& result : results) {
      record(*metrics, result);
    }

    const bool dependencyFailed = configuration.status == DoctorCheckStatus::Failed ||
                                  driver.status == DoctorCheckStatus::Failed ||
                                  connection.status == DoctorCheckStatus::Failed;
    const ExecutionStatus status =
      metrics->checksFailed == 0 && !dependencyFailed ? ExecutionStatus::Success : ExecutionStatus::Failed;

    return {
      .info = status == ExecutionStatus::Success ? "Doctor checks completed." : "Doctor found environment problems.",
      .status = status,
      .metrics = metrics,
      .renderedOutput = renderResults(invocation, results, *metrics),
    };
  }
} // namespace worm::cli::database
