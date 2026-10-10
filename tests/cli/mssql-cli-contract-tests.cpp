#include "../connection/drivers/mssql-test-support.hpp"

#include <database/diff.hpp>
#include <database/doctor.hpp>
#include <database/inspect.hpp>
#include <database/migrate.hpp>
#include <database/seed.hpp>
#include <generator/check.hpp>
#include <generator/pull.hpp>
#include <generator/push.hpp>
#include <helpers/file.hpp>
#include <helpers/manifest.hpp>
#include <helpers/migration/migration-file.hpp>
#include <parser.hpp>
#include <validator.hpp>

#include <core/model/migration-artifact.hpp>
#include <errors/unknown-argument-exception.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  constexpr int skippedTest = 77;

  class Fixture
  {
  public:
    Fixture()
      : path_(std::filesystem::temp_directory_path() / "worm-mssql-cli-contract"),
        manifest_(path_ / "worm-schema.json"),
        entities_(path_ / "entities"),
        migrations_(path_ / "migrations"),
        config_(worm::tests::mssql::connectionConfig()),
        database_(config_)
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
      std::filesystem::create_directories(entities_);
      std::filesystem::create_directories(migrations_);

      database_.execute(
        "DECLARE @sql NVARCHAR(MAX) = N''; "
        "SELECT @sql += N'ALTER TABLE ' + QUOTENAME(OBJECT_SCHEMA_NAME(parent_object_id)) + N'.' + "
        "QUOTENAME(OBJECT_NAME(parent_object_id)) + N' DROP CONSTRAINT ' + QUOTENAME(name) + N';' "
        "FROM sys.foreign_keys WHERE is_ms_shipped = 0; EXEC sys.sp_executesql @sql; "
        "SET @sql = N''; SELECT @sql += N'DROP TABLE ' + QUOTENAME(SCHEMA_NAME(schema_id)) + N'.' + "
        "QUOTENAME(name) + N';' FROM sys.tables WHERE is_ms_shipped = 0; EXEC sys.sp_executesql @sql;");

      const worm::cli::SchemaManifest
        schema{
          .entities =
            {
              {
                .name = "Role",
                .table =
                  {
                    .schema = "dbo",
                    .name = "worm_cli_roles",
                    .columns =
                      {
                        {
                          .name = "id",
                          .type = {.kind = worm::core::ColumnTypeKind::Int64},
                          .nullable = false,
                        },
                        {
                          .name = "name",
                          .type = {.kind = worm::core::ColumnTypeKind::String, .length = 64},
                          .nullable = false,
                          .unique = true,
                        },
                      },
                    .primaryKey = {"id"},
                  },
              },
              {
                .name = "User",
                .table =
                  {
                    .schema = "dbo",
                    .name = "worm_cli_users",
                    .columns =
                      {
                        {
                          .name = "id",
                          .type = {.kind = worm::core::ColumnTypeKind::Int64},
                          .nullable = false,
                        },
                        {
                          .name = "role_id",
                          .type = {.kind = worm::core::ColumnTypeKind::Int64},
                          .nullable = false,
                        },
                        {
                          .name = "email",
                          .type = {.kind = worm::core::ColumnTypeKind::String, .length = 255},
                          .nullable = false,
                          .unique = true,
                        },
                      },
                    .primaryKey = {"id"},
                  },
                .indexes =
                  {
                    {
                      .name = "ix_worm_cli_users_role",
                      .columns = {{.name = "role_id"}},
                    },
                  },
                .foreignKeys =
                  {
                    {
                      .name = "fk_worm_cli_users_role",
                      .columns = {"role_id"},
                      .referencedSchema = "dbo",
                      .referencedTable = "worm_cli_roles",
                      .referencedColumns = {"id"},
                    },
                  },
              },
            },
        };
      worm::cli::writeGeneratedFile(manifest_, worm::cli::serializeManifest(schema));
    }

    ~Fixture()
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]]
    worm::cli::Invocation invocation(std::vector<std::string> command) const
    {
      std::vector<std::string> arguments{
        "--manifest",
        manifest_.string(),
        "--driver",
        "mssql",
        "--host",
        config_.host,
        "--port",
        config_.port,
        "--database",
        config_.dbname,
        "--username",
        config_.username,
        "--password-env",
        "WORM_TEST_MSSQL_PASSWORD",
        "--trust-server-certificate",
      };
      arguments.insert(arguments.end(), command.begin(), command.end());
      worm::cli::Invocation result = worm::cli::parse(arguments);
      worm::cli::resolve(result);
      worm::cli::validate(result);
      return result;
    }

    [[nodiscard]]
    const std::filesystem::path& entities() const noexcept
    {
      return entities_;
    }

    [[nodiscard]]
    const std::filesystem::path& migrations() const noexcept
    {
      return migrations_;
    }

    [[nodiscard]]
    long long count(std::string table) const
    {
      return database_.scalar("SELECT COUNT(*) FROM [dbo].[" + table + "]");
    }

    void execute(std::string sql) const
    {
      database_.execute(std::move(sql));
    }

  private:
    std::filesystem::path path_;
    std::filesystem::path manifest_;
    std::filesystem::path entities_;
    std::filesystem::path migrations_;
    worm::connection::ConnectionConfig config_;
    worm::tests::mssql::OdbcConnection database_;
  };

  void require(bool condition, std::string message)
  {
    if (!condition) {
      throw std::runtime_error(std::move(message));
    }
  }
} // namespace

int main()
try {
  if (worm::tests::mssql::environmentValue("WORM_TEST_MSSQL_DBNAME").empty() ||
      worm::tests::mssql::environmentValue("WORM_TEST_MSSQL_PASSWORD").empty()) {
    return skippedTest;
  }

  const Fixture fixture;

  const auto push = worm::cli::generator::push(fixture.invocation({"push", "--apply"}));
  const auto pushMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(push.metrics);
  require(
    push.status == worm::cli::ExecutionStatus::Success && pushMetrics != nullptr && pushMetrics->createdTables == 2 &&
      pushMetrics->failedTables == 0,
    "MSSQL push did not create the schema and constraints.");

  const auto repeatedPush = worm::cli::generator::push(fixture.invocation({"push", "--apply"}));
  const auto repeatedPushMetrics =
    std::dynamic_pointer_cast<const worm::cli::generator::PushMetrics>(repeatedPush.metrics);
  require(
    repeatedPush.status == worm::cli::ExecutionStatus::Success && repeatedPushMetrics != nullptr &&
      repeatedPushMetrics->compatibleTables == 2 && repeatedPushMetrics->createdTables == 0,
    "Repeated MSSQL push did not recognize the existing compatible schema.");

  const auto check = worm::cli::generator::check(fixture.invocation({"check"}));
  const auto checkMetrics = std::dynamic_pointer_cast<const worm::cli::generator::CheckMetrics>(check.metrics);
  require(
    check.status == worm::cli::ExecutionStatus::Success && checkMetrics != nullptr &&
      checkMetrics->incompatibleObjects == 0 && checkMetrics->missingInCode == 0 &&
      checkMetrics->missingInDatabase == 0,
    "MSSQL check did not recognize the pushed schema as compatible.");

  const auto inspect = worm::cli::database::inspect(fixture.invocation({"inspect"}));
  const auto inspectMetrics = std::dynamic_pointer_cast<const worm::cli::database::InspectMetrics>(inspect.metrics);
  require(
    inspect.status == worm::cli::ExecutionStatus::Success && inspectMetrics != nullptr &&
      inspectMetrics->tablesDiscovered == 2 && inspectMetrics->foreignKeysDiscovered == 1 &&
      inspectMetrics->indexesDiscovered == 1,
    "MSSQL inspect did not discover tables, the foreign key, and the index.");

  const auto diff = worm::cli::database::diff(fixture.invocation({"diff"}));
  const auto diffMetrics = std::dynamic_pointer_cast<const worm::cli::database::DiffMetrics>(diff.metrics);
  require(
    diff.status == worm::cli::ExecutionStatus::Success && diffMetrics != nullptr &&
      diffMetrics->differencesDetected == 0,
    "MSSQL diff reported drift immediately after push.");

  fixture.execute("ALTER TABLE [dbo].[worm_cli_users] ADD [legacy] INT NULL");
  const auto drift = worm::cli::database::diff(fixture.invocation({"diff"}));
  const auto driftMetrics = std::dynamic_pointer_cast<const worm::cli::database::DiffMetrics>(drift.metrics);
  require(
    drift.status == worm::cli::ExecutionStatus::DriftDetected && driftMetrics != nullptr &&
      driftMetrics->unexpectedColumns == 1,
    "MSSQL diff did not detect an unexpected database column.");
  fixture.execute("ALTER TABLE [dbo].[worm_cli_users] DROP COLUMN [legacy]");

  const auto pull = worm::cli::generator::pull(fixture.invocation(
    {"pull", "--table", "worm_cli_users", "--name", "MssqlUser", "--output", fixture.entities().string(), "--apply"}));
  const auto pullMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PullMetrics>(pull.metrics);
  require(
    pull.status == worm::cli::ExecutionStatus::Success && pullMetrics != nullptr &&
      pullMetrics->generatedEntities == 1 && std::filesystem::is_regular_file(fixture.entities() / "mssql-user.hpp"),
    "MSSQL pull did not generate the selected entity.");

  const auto seed = worm::cli::database::seed(fixture.invocation({"seed", "--all", "--rows", "2", "--apply"}));
  const auto seedMetrics = std::dynamic_pointer_cast<const worm::cli::database::SeedMetrics>(seed.metrics);
  require(
    seed.status == worm::cli::ExecutionStatus::Success && seedMetrics != nullptr && seedMetrics->insertedRows == 4 &&
      fixture.count("worm_cli_roles") == 2 && fixture.count("worm_cli_users") == 2,
    "MSSQL seed did not populate related tables.");

  const worm::core::MigrationArtifact artifact = worm::core::makeMigrationArtifact(
    "20261010000100",
    "mssql-cli-contract",
    "mssql",
    {{.description = "Create CLI migration table",
      .sql = "CREATE TABLE [dbo].[worm_cli_migration] ([id] BIGINT PRIMARY KEY)"}},
    std::vector<worm::core::MigrationStatement>{
      {.description = "Drop CLI migration table",
        .sql = "DROP TABLE [dbo].[worm_cli_migration]",
        .risk = worm::core::MigrationRisk::Destructive},
    });
  worm::cli::migration::saveMigrationArtifact(
    fixture.migrations() / "20261010000100_mssql-cli-contract.worm.json",
    artifact);

  const auto migrate = worm::cli::database::migrate(
    fixture.invocation({"migrate", "--apply", "--directory", fixture.migrations().string()}));
  const auto migrateMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationApplyMetrics>(migrate.metrics);
  require(
    migrate.status == worm::cli::ExecutionStatus::Success && migrateMetrics != nullptr &&
      migrateMetrics->appliedMigrations == 1 && fixture.count("worm_cli_migration") == 0,
    "MSSQL migrate did not apply the migration artifact.");

  const auto rollback = worm::cli::database::migrate(
    fixture.invocation({"migrate", "--rollback", "--directory", fixture.migrations().string()}));
  const auto rollbackMetrics =
    std::dynamic_pointer_cast<const worm::cli::database::MigrationRollbackMetrics>(rollback.metrics);
  require(
    rollback.status == worm::cli::ExecutionStatus::Success && rollbackMetrics != nullptr &&
      rollbackMetrics->rolledBackMigrations == 1,
    "MSSQL migrate did not roll back the latest migration.");

  const auto doctor = worm::cli::database::examine(fixture.invocation({"doctor"}));
  const auto doctorMetrics = std::dynamic_pointer_cast<const worm::cli::database::DoctorMetrics>(doctor.metrics);
  require(
    doctor.status != worm::cli::ExecutionStatus::Failed && doctorMetrics != nullptr &&
      doctorMetrics->checksTotal == 5 && doctorMetrics->checksFailed == 0,
    "MSSQL doctor reported a failed validation check.");

  bool syncRejected = false;
  try {
    static_cast<void>(worm::cli::parse({"sync"}));
  } catch (const worm::cli::UnknownArgumentException&) {
    syncRejected = true;
  }
  require(syncRejected, "The unsupported sync command was unexpectedly accepted.");
  return 0;
} catch (const std::exception& error) {
  std::cerr << "MSSQL CLI contract failed: " << error.what() << '\n';
  return 1;
}
