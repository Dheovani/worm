#include <generator/pull.hpp>

#include <errors/invalid-cli-argument-exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

namespace
{
  class TemporaryDirectory
  {
  public:
    TemporaryDirectory()
      : path_(std::filesystem::temp_directory_path() / "worm-cli-pull-tests")
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };

  worm::core::SchemaSnapshot schema(worm::core::ColumnTypeKind valueType = worm::core::ColumnTypeKind::String)
  {
    return {{
      {
        .schema = "public",
        .name = "user_records",
        .columns =
          {
            {
              .name = "id",
              .type = {.kind = worm::core::ColumnTypeKind::Int64, .nativeName = "int8"},
              .nullable = false,
              .generated = true,
            },
            {
              .name = "display_name",
              .type = {.kind = valueType,
                .nativeName = valueType == worm::core::ColumnTypeKind::String ? "varchar" : "numeric"},
              .defaultExpression = "'unknown'",
              .nullable = true,
              .unique = true,
            },
          },
        .primaryKey = {"id"},
      },
    }};
  }
} // namespace

int main()
try {
  const TemporaryDirectory temporary;
  worm::cli::Invocation invocation{.command = worm::cli::Commands::Pull};
  invocation.arguments.output = temporary.path().string();
  invocation.arguments.namespaceName = "application::entities";

  const auto planned = worm::cli::generator::pull(invocation, schema());
  const auto plannedMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PullMetrics>(planned.metrics);
  if (planned.status != worm::cli::ExecutionStatus::Success || plannedMetrics == nullptr ||
      plannedMetrics->plannedEntities != 1 || plannedMetrics->generatedEntities != 0 ||
      planned.info.find("user_records ->") == std::string::npos || std::filesystem::exists(temporary.path())) {
    std::cerr << "Pull dry-run modified the filesystem or produced invalid metrics.\n";
    return 1;
  }

  invocation.arguments.apply = true;
  const auto applied = worm::cli::generator::pull(invocation, schema());
  const auto appliedMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PullMetrics>(applied.metrics);
  const auto entityPath = temporary.path() / "user-records.hpp";
  std::ifstream stream{entityPath};
  std::ostringstream contents;
  contents << stream.rdbuf();
  const bool entityRead = static_cast<bool>(stream);
  stream.close();
  if (applied.status != worm::cli::ExecutionStatus::Success || appliedMetrics == nullptr ||
      appliedMetrics->generatedEntities != 1 || !entityRead ||
      contents.str().find("struct UserRecords") == std::string::npos ||
      contents.str().find("std::optional<std::string> displayName") == std::string::npos ||
      contents.str().find("#include <core/query/parameter-value.hpp>") == std::string::npos ||
      contents.str().find(".defaultExpression = \"'unknown'\"") == std::string::npos ||
      contents.str().find("namespace application::entities") == std::string::npos) {
    std::cerr << "Pull did not generate the expected entity.\n";
    return 1;
  }

  const auto repeated = worm::cli::generator::pull(invocation, schema());
  const auto repeatedMetrics = std::dynamic_pointer_cast<const worm::cli::generator::PullMetrics>(repeated.metrics);
  if (repeated.status != worm::cli::ExecutionStatus::Success || repeatedMetrics == nullptr ||
      repeatedMetrics->existingEntities != 1 || repeatedMetrics->generatedEntities != 0) {
    std::cerr << "Pull overwrote an existing entity.\n";
    return 1;
  }

  using ColumnTypeKind = worm::core::ColumnTypeKind;
  const auto verifyMappedType =
    [&](ColumnTypeKind kind, std::string_view name, std::string_view file, std::string_view type) {
      invocation.arguments.name = std::string{name};
      const auto report = worm::cli::generator::pull(invocation, schema(kind));
      const auto path = temporary.path() / file;
      std::ifstream generated{path};
      std::ostringstream generatedContents;
      generatedContents << generated.rdbuf();
      const bool valid = report.status == worm::cli::ExecutionStatus::Success &&
                         generatedContents.str().find(std::string{type} + " displayName") != std::string::npos;
      generated.close();
      std::filesystem::remove(path);
      return valid;
    };

  if (!verifyMappedType(
        worm::core::ColumnTypeKind::Date,
        "DateRecord",
        "date-record.hpp",
        "std::optional<std::chrono::sys_days>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::Time,
        "TimeRecord",
        "time-record.hpp",
        "std::optional<std::string>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::DateTime,
        "DateTimeRecord",
        "date-time-record.hpp",
        "std::optional<std::string>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::Uuid,
        "UuidRecord",
        "uuid-record.hpp",
        "std::optional<std::string>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::Json,
        "JsonRecord",
        "json-record.hpp",
        "std::optional<std::string>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::Decimal,
        "DecimalRecord",
        "decimal-record.hpp",
        "std::optional<worm::core::Decimal>") ||
      !verifyMappedType(
        worm::core::ColumnTypeKind::Binary,
        "BinaryRecord",
        "binary-record.hpp",
        "std::optional<worm::core::Binary>")) {
    std::cerr << "Pull did not preserve the supported SQL type representations.\n";
    return 1;
  }

  auto enumSchema = schema(worm::core::ColumnTypeKind::Enum);
  enumSchema.tables[0].columns[1].type = {
    .kind = worm::core::ColumnTypeKind::Enum,
    .nativeName = "account_status",
    .enumeration =
      worm::core::NativeEnum{
        .schema = "public",
        .name = "account_status",
        .values = {"active", "on-hold"},
      },
  };
  invocation.arguments.name = "EnumRecord";
  const auto enumReport = worm::cli::generator::pull(invocation, enumSchema);
  std::ifstream enumFile{temporary.path() / "enum-record.hpp"};
  std::ostringstream enumContents;
  enumContents << enumFile.rdbuf();
  if (enumReport.status != worm::cli::ExecutionStatus::Success ||
      enumContents.str().find("std::optional<std::string> displayName") == std::string::npos ||
      enumContents.str().find("displayNameEnumSchema{\"public\"}") == std::string::npos ||
      enumContents.str().find("displayNameEnumName{\"account_status\"}") == std::string::npos ||
      enumContents.str().find("displayNameValues") == std::string::npos ||
      enumContents.str().find("\"active\"") == std::string::npos ||
      enumContents.str().find("\"on-hold\"") == std::string::npos) {
    std::cerr << "Pull did not preserve the native enum definition.\n";
    return 1;
  }
  enumFile.close();
  std::filesystem::remove(temporary.path() / "enum-record.hpp");

  invocation.arguments.name.reset();

  const auto unsupported = worm::cli::generator::pull(invocation, schema(worm::core::ColumnTypeKind::Unknown));
  if (unsupported.status != worm::cli::ExecutionStatus::Success) {
    // The existing file is intentionally skipped. Use a different explicit entity name to exercise type rejection.
    std::cerr << "Existing entity handling changed unexpectedly.\n";
    return 1;
  }

  std::filesystem::remove(entityPath);
  const auto rejectedType = worm::cli::generator::pull(invocation, schema(worm::core::ColumnTypeKind::Unknown));
  if (rejectedType.status != worm::cli::ExecutionStatus::Failed ||
      rejectedType.info.find("unsupported SQL type 'numeric'") == std::string::npos) {
    std::cerr << "Unsupported lossy type was accepted.\n";
    return 1;
  }

  invocation.arguments.tables = {"missing"};
  try {
    static_cast<void>(worm::cli::generator::pull(invocation, schema()));
  } catch (const worm::cli::InvalidCliArgumentException&) {
    return 0;
  }

  std::cerr << "Unknown table selection was accepted.\n";
  return 1;
} catch (const std::exception& error) {
  std::cerr << "Unexpected pull exception: " << error.what() << '\n';
  return 1;
}
