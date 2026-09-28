#include <database/doctor.hpp>
#include <errors/query-execution-exception.hpp>
#include <parser.hpp>
#include <runner.hpp>
#include <validator.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
  struct FakeState
  {
    std::string version{"3.40.0"};
    worm::connection::DatabasePermissions permissions;
    std::vector<std::string> statements;
    std::string connectionError{"Connection refused."};
    bool connectionFails{};
    bool versionFails{};
    bool permissionsFail{};
    std::size_t clientsCreated{};
  };

  worm::connection::DatabasePermissions grantedPermissions()
  {
    using enum worm::connection::DatabasePermission;
    using worm::connection::DatabasePermissionCheck;
    using worm::connection::DatabasePermissionStatus;
    return {DatabasePermissionCheck{Select, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{Insert, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{Update, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{Delete, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{CreateTable, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{AlterTable, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{DropTable, DatabasePermissionStatus::Granted},
      DatabasePermissionCheck{CreateIndex, DatabasePermissionStatus::Granted}};
  }

  class FakeClient final : public worm::connection::Client
  {
  public:
    explicit FakeClient(std::shared_ptr<FakeState> state)
      : state_(std::move(state))
    {}

    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

  private:
    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      state_->statements.push_back(statement.sql);
      if (state_->connectionFails) {
        throw worm::QueryExecutionException(state_->connectionError);
      }
      return worm::core::ResultSet{{worm::core::ResultRow{{{"value", std::int64_t{1}}}}}};
    }

    std::string databaseVersionImpl() override
    {
      if (state_->versionFails) {
        throw worm::QueryExecutionException("Version query failed.");
      }
      return state_->version;
    }

    worm::connection::DatabasePermissions databasePermissionsImpl() override
    {
      if (state_->permissionsFail) {
        throw worm::QueryExecutionException("Permission query failed.");
      }
      return state_->permissions;
    }

    void beginTransactionImpl() override {}
    void rollbackTransactionImpl() override {}
    void commitTransactionImpl() override {}

    std::shared_ptr<FakeState> state_;
  };

  worm::cli::Invocation invocation(std::vector<std::string> arguments)
  {
    worm::cli::Invocation value = worm::cli::parse(arguments);
    worm::cli::validate(value);
    return value;
  }

  worm::cli::database::DoctorDependencies dependencies(const std::shared_ptr<FakeState>& state, bool driver = true)
  {
    return {
      .clientFactory =
        [state](const auto&, auto) {
          ++state->clientsCreated;
          return std::make_unique<FakeClient>(state);
        },
      .driverAvailable = [driver](auto) { return driver; },
    };
  }

  std::shared_ptr<const worm::cli::database::DoctorMetrics> metrics(const worm::cli::ExecutionReport& report)
  {
    return std::dynamic_pointer_cast<const worm::cli::database::DoctorMetrics>(report.metrics);
  }

  bool hasOutput(const worm::cli::ExecutionReport& report, std::string_view value)
  {
    return report.renderedOutput.has_value() && report.renderedOutput->find(value) != std::string::npos;
  }
} // namespace

int main()
{
  using worm::cli::DoctorCheck;
  using worm::cli::ExecutionStatus;
  using worm::cli::database::examine;

  const auto parsed =
    worm::cli::parse({"doctor", "--config", "--driver", "--connection", "--version", "--permissions", "--verbose"});
  if (parsed.command != worm::cli::Commands::Doctor || !parsed.global.verbose ||
      parsed.doctorChecks != std::vector<DoctorCheck>{DoctorCheck::Configuration,
                               DoctorCheck::Driver,
                               DoctorCheck::Connection,
                               DoctorCheck::Version,
                               DoctorCheck::Permissions}) {
    std::cerr << "Doctor filters were not parsed correctly.\n";
    return 1;
  }

  bool duplicateRejected = false;
  bool unrelatedOptionRejected = false;
  try {
    static_cast<void>(worm::cli::parse({"doctor", "--connection", "--connection"}));
  } catch (const std::exception&) {
    duplicateRejected = true;
  }
  try {
    worm::cli::validate(worm::cli::parse({"doctor", "--apply"}));
  } catch (const std::exception&) {
    unrelatedOptionRejected = true;
  }
  if (!duplicateRejected || !unrelatedOptionRejected) {
    std::cerr << "Doctor accepted duplicate or unrelated command options.\n";
    return 1;
  }

  auto state = std::make_shared<FakeState>();
  state->permissions = grantedPermissions();
  const auto all = examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor"}), dependencies(state));
  const auto allMetrics = metrics(all);
  if (all.status != ExecutionStatus::Success || allMetrics == nullptr || allMetrics->checksTotal != 5 ||
      allMetrics->checksPassed != 5 || state->statements != std::vector<std::string>{"SELECT 1"} ||
      !hasOutput(all, "[PASS] Configuration") || !hasOutput(all, "[PASS] Permissions")) {
    std::cerr << "Default doctor execution did not run all checks in read-only mode.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  state->permissions = grantedPermissions();
  const auto configOnly =
    examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--config"}), dependencies(state));
  if (metrics(configOnly)->checksTotal != 1 || metrics(configOnly)->checksPassed != 1 || state->clientsCreated != 0) {
    std::cerr << "Doctor configuration filter executed unrelated checks.\n";
    return 1;
  }

  const auto missingConfig = examine(invocation({"doctor"}), dependencies(std::make_shared<FakeState>()));
  if (missingConfig.status != ExecutionStatus::Failed || metrics(missingConfig)->checksFailed != 1 ||
      metrics(missingConfig)->checksSkipped != 4) {
    std::cerr << "Doctor did not skip checks that depend on invalid configuration.\n";
    return 1;
  }

  const auto filteredMissingConfig =
    examine(invocation({"doctor", "--version"}), dependencies(std::make_shared<FakeState>()));
  if (filteredMissingConfig.status != ExecutionStatus::Failed || metrics(filteredMissingConfig)->checksFailed != 0 ||
      metrics(filteredMissingConfig)->checksSkipped != 1 || !hasOutput(filteredMissingConfig, "[SKIP] Version")) {
    std::cerr << "Doctor did not preserve a skipped filtered check after its hidden dependency failed.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  const auto missingDriver =
    examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor"}), dependencies(state, false));
  if (missingDriver.status != ExecutionStatus::Failed || metrics(missingDriver)->checksFailed != 1 ||
      metrics(missingDriver)->checksSkipped != 3 || state->clientsCreated != 0) {
    std::cerr << "Doctor confused an unavailable driver with a failed connection.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  state->connectionFails = true;
  const auto failedConnection =
    examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor"}), dependencies(state));
  if (metrics(failedConnection)->checksFailed != 1 || metrics(failedConnection)->checksSkipped != 2 ||
      !hasOutput(failedConnection, "Connection: Database connection failed")) {
    std::cerr << "Doctor did not skip version and permissions after a connection failure.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  state->permissions = grantedPermissions();
  state->version = "4.0.0";
  const auto warning =
    examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--version"}), dependencies(state));
  if (warning.status != ExecutionStatus::Success || metrics(warning)->checksWarned != 1 ||
      worm::cli::exitCode(warning.status) != 0) {
    std::cerr << "Doctor warning incorrectly caused a failing exit status.\n";
    return 1;
  }

  state->version = "3.34.0";
  const auto unsupported =
    examine(invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--version"}), dependencies(state));
  if (unsupported.status != ExecutionStatus::Failed || metrics(unsupported)->checksFailed != 1 ||
      worm::cli::exitCode(unsupported.status) != 1) {
    std::cerr << "Doctor accepted an unsupported database version.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  state->permissions = grantedPermissions();
  state->permissions.front().status = worm::connection::DatabasePermissionStatus::Denied;
  const auto denied = examine(
    invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--permissions"}),
    dependencies(state));
  state->permissions.front().status = worm::connection::DatabasePermissionStatus::Unknown;
  const auto unknown = examine(
    invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--permissions"}),
    dependencies(state));
  if (denied.status != ExecutionStatus::Failed || unknown.status != ExecutionStatus::Success ||
      metrics(unknown)->checksWarned != 1) {
    std::cerr << "Doctor permission statuses are inconsistent.\n";
    return 1;
  }

  state->permissionsFail = true;
  const auto permissionQueryFailure = examine(
    invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--permissions"}),
    dependencies(state));
  if (permissionQueryFailure.status != ExecutionStatus::Failed || metrics(permissionQueryFailure)->checksFailed != 1) {
    std::cerr << "Doctor ignored a permission catalog failure.\n";
    return 1;
  }

  const auto verbose = examine(
    invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--config", "--verbose"}),
    dependencies(state));
  if (!hasOutput(verbose, "driver=sqlite") || hasOutput(verbose, "password")) {
    std::cerr << "Doctor verbose diagnostics are missing or expose sensitive fields.\n";
    return 1;
  }

  state = std::make_shared<FakeState>();
  state->connectionFails = true;
  state->connectionError = "Connection rejected password secret-value.";
  auto secretInvocation =
    invocation({"--driver", "sqlite", "--database", ":memory:", "doctor", "--connection", "--verbose"});
  secretInvocation.global.password = "secret-value";
  const auto sanitized = examine(secretInvocation, dependencies(state));
  if (hasOutput(sanitized, "secret-value") || !hasOutput(sanitized, "<redacted>")) {
    std::cerr << "Doctor verbose diagnostics exposed a connection secret.\n";
    return 1;
  }

  return 0;
}
