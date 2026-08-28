#include "pull.hpp"

#include <core/model/schema-snapshot.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "../errors/entity-creation-exception.hpp"
#include "../errors/invalid-cli-argument-exception.hpp"
#include "../validator.hpp"
#include <helpers/connection.hpp>
#include <helpers/file.hpp>

namespace worm::cli::generator
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    struct GenerationFailure
    {
      std::string table;
      std::string reason;
    };

    struct GenerationPlan
    {
      std::string table;
      std::filesystem::path path;
      std::string contents;
    };

    [[nodiscard]]
    std::string escaped(std::string_view value)
    {
      std::string result;
      result.reserve(value.size());
      for (const char character : value) {
        if (character == '\\' || character == '"')
          result.push_back('\\');
        result.push_back(character);
      }
      return result;
    }

    [[nodiscard]]
    std::vector<std::string> words(std::string_view value)
    {
      std::vector<std::string> result;
      std::string current;
      for (const unsigned char character : value) {
        if (std::isalnum(character)) {
          current.push_back(static_cast<char>(character));
        } else if (!current.empty()) {
          result.push_back(std::move(current));
          current.clear();
        }
      }
      if (!current.empty())
        result.push_back(std::move(current));
      return result;
    }

    [[nodiscard]]
    std::string pascalCase(std::string_view value)
    {
      std::string result;
      for (auto word : words(value)) {
        word.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(word.front())));
        result += word;
      }
      if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())))
        result.insert(0, "Entity");
      return result;
    }

    [[nodiscard]]
    std::string camelCase(std::string_view value)
    {
      std::string result = pascalCase(value);
      result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front())));
      if (isCppKeyword(result))
        result.push_back('_');
      return result;
    }

    [[nodiscard]]
    std::string fileName(std::string_view entityName)
    {
      std::string result;
      for (std::size_t index = 0; index < entityName.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(entityName[index]);
        if (std::isupper(character) && index != 0)
          result.push_back('-');
        result.push_back(static_cast<char>(std::tolower(character)));
      }
      return result + ".hpp";
    }

    [[nodiscard]]
    std::string cppType(const core::SchemaColumnSnapshot& column)
    {
      std::string type;
      switch (column.type.kind) {
      case core::ColumnTypeKind::Boolean:
        type = "bool";
        break;
      case core::ColumnTypeKind::Int16:
        type = column.type.unsignedValue ? "std::uint16_t" : "std::int16_t";
        break;
      case core::ColumnTypeKind::Int32:
        type = column.type.unsignedValue ? "std::uint32_t" : "std::int32_t";
        break;
      case core::ColumnTypeKind::Int64:
        if (column.type.unsignedValue) {
          throw EntityCreationException(
            "unsigned 64-bit column '{}' cannot be represented without possible data loss",
            column.name);
        }
        type = "std::int64_t";
        break;
      case core::ColumnTypeKind::Float32:
        type = "float";
        break;
      case core::ColumnTypeKind::Float64:
        type = "double";
        break;
      case core::ColumnTypeKind::String:
        type = "std::string";
        break;
      case core::ColumnTypeKind::Date:
        type = "std::chrono::sys_days";
        break;
      default:
        throw EntityCreationException(
          "column '{}' uses unsupported SQL type '{}' ({})",
          column.name,
          column.type.nativeName,
          core::columnTypeKindName(column.type.kind));
      }
      return column.nullable ? "std::optional<" + type + ">" : type;
    }

    [[nodiscard]]
    std::string generateFileContents(
      const core::SchemaTableSnapshot& table,
      std::string_view entityName,
      const std::optional<std::string>& namespaceName)
    {
      if (table.primaryKey.size() != 1) {
        throw EntityCreationException(
          "table '{}' must have exactly one primary-key column to generate a persistable entity",
          table.name);
      }

      std::ostringstream out;
      out << "#pragma once\n\n"
             "#include <core/model/constraint.hpp>\n"
             "#include <core/model/schema.hpp>\n"
             "#include <reflection/field.hpp>\n\n"
             "#include <chrono>\n"
             "#include <cstdint>\n"
             "#include <optional>\n"
             "#include <string>\n"
             "#include <tuple>\n";
      if (namespaceName.has_value())
        out << "\nnamespace " << *namespaceName << "\n{\n";

      const std::string indent = namespaceName.has_value() ? "  " : "";
      out << '\n' << indent << "struct " << entityName << "\n" << indent << "{\n";

      std::vector<std::string> memberNames;
      std::unordered_set<std::string> uniqueMemberNames;
      memberNames.reserve(table.columns.size());
      for (const auto& column : table.columns) {
        const std::string memberName = camelCase(column.name);
        if (!uniqueMemberNames.insert(memberName).second) {
          throw EntityCreationException(
            "columns of table '{}' produce duplicate C++ member name '{}'",
            table.name,
            memberName);
        }
        memberNames.push_back(memberName);
        out << indent << "  " << cppType(column) << ' ' << memberName << "{};\n";
      }

      out << "\n"
          << indent << "  static constexpr worm::core::Table table() noexcept\n"
          << indent << "  {\n"
          << indent << "    return worm::core::Table{";
      if (!table.schema.empty())
        out << "worm::core::Schema{\"" << escaped(table.schema) << "\"}, ";
      out << "\"" << escaped(table.name) << "\"};\n"
          << indent << "  }\n\n"
          << indent << "  static constexpr worm::core::PrimaryKey primaryKey() noexcept\n"
          << indent << "  {\n"
          << indent << "    return worm::core::PrimaryKey{\"pk_" << escaped(table.name) << "\", {worm::core::Column{\""
          << escaped(table.primaryKey.front()) << "\", table()}}};\n"
          << indent << "  }\n\n"
          << indent << "  static constexpr auto reflect() noexcept\n"
          << indent << "  {\n"
          << indent << "    return std::tuple{\n";

      for (std::size_t index = 0; index < table.columns.size(); ++index) {
        const auto& column = table.columns[index];
        out << indent << "      worm::reflection::field(\"" << memberNames[index] << "\", &" << entityName
            << "::" << memberNames[index] << ", worm::reflection::FieldMetadata{.columnName = \""
            << escaped(column.name) << "\", .generated = " << (column.generated ? "true" : "false")
            << ", .unique = " << (column.unique ? "true" : "false")
            << ", .nullable = " << (column.nullable ? "true" : "false") << "})"
            << (index + 1 == table.columns.size() ? "};\n" : ",\n");
      }

      out << indent << "  }\n" << indent << "};\n";
      if (namespaceName.has_value())
        out << "} // namespace " << *namespaceName << "\n";
      return out.str();
    }

    [[nodiscard]]
    std::filesystem::path outputDirectory(const Invocation& invocation)
    {
      return invocation.arguments.output.value_or(std::filesystem::current_path().string());
    }

    [[nodiscard]]
    std::string
    reportInfo(const std::vector<GenerationFailure>& failures, const std::vector<GenerationPlan>& plans, bool applied)
    {
      std::string message;
      if (!failures.empty()) {
        message = "Entity generation failed:\n";
        for (const auto& failure : failures)
          message += "  - " + failure.table + ": " + failure.reason + "\n";
        return message;
      }

      if (applied)
        return "Entity generation completed successfully.";

      message = plans.empty() ? "No entity files need to be generated." : "Entity generation plan:\n";
      for (const auto& plan : plans)
        message += "  - " + plan.table + " -> " + plan.path.string() + "\n";
      if (!plans.empty())
        message += "Run again with --apply to write these files.";
      return message;
    }

    [[nodiscard]]
    std::vector<const core::SchemaTableSnapshot*>
    selectedTables(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema)
    {
      std::vector<const core::SchemaTableSnapshot*> selected;
      for (const auto& table : databaseSchema.tables) {
        if (invocation.arguments.tables.empty() ||
            std::ranges::find(invocation.arguments.tables, table.name) != invocation.arguments.tables.end()) {
          selected.push_back(&table);
        }
      }
      for (const auto& requested : invocation.arguments.tables) {
        const bool found = std::ranges::any_of(selected, [&](const auto* table) { return table->name == requested; });
        if (!found)
          throw InvalidCliArgumentException("Unknown table '{}'.", requested);
      }
      return selected;
    }

    [[nodiscard]]
    ExecutionReport createFiles(
      const Invocation& invocation,
      const core::SchemaSnapshot& databaseSchema,
      const std::shared_ptr<PullMetrics>& metrics)
    {
      const auto planningStarted = Clock::now();
      metrics->tablesDiscovered = databaseSchema.tables.size();
      const auto tables = selectedTables(invocation, databaseSchema);
      metrics->tablesSelected = tables.size();

      std::vector<GenerationFailure> failures;
      std::vector<GenerationPlan> plans;
      std::unordered_set<std::string> plannedPaths;

      for (const auto* table : tables) {
        const std::string entityName = invocation.arguments.name.has_value() && tables.size() == 1
                                         ? *invocation.arguments.name
                                         : pascalCase(table->name);
        const auto path = outputDirectory(invocation) / fileName(entityName);

        if (std::filesystem::exists(path)) {
          ++metrics->existingEntities;
          continue;
        }

        if (!plannedPaths.insert(path.lexically_normal().generic_string()).second) {
          failures.push_back({table->name, "generated file path collides with another selected table"});
          continue;
        }

        try {
          plans.push_back(
            {
              table->name,
              path,
              generateFileContents(*table, entityName, invocation.arguments.namespaceName),
            });
        } catch (const std::exception& error) {
          failures.push_back({table->name, error.what()});
        }
      }

      metrics->missingEntities = plans.size();
      metrics->plannedEntities = plans.size();
      metrics->failedEntities = failures.size();
      metrics->planningDuration = Clock::now() - planningStarted;

      if (invocation.arguments.apply && failures.empty() && !plans.empty()) {
        const auto executionStarted = Clock::now();
        for (const auto& plan : plans) {
          try {
            writeGeneratedFile(plan.path, plan.contents);
            ++metrics->generatedEntities;
          } catch (const std::exception& error) {
            failures.push_back({plan.path.filename().string(), error.what()});
            ++metrics->failedEntities;
          }
        }

        metrics->executionDuration = Clock::now() - executionStarted;
      }

      return {
        .info = reportInfo(failures, plans, invocation.arguments.apply),
        .status = failures.empty() ? ExecutionStatus::Success : ExecutionStatus::Failed,
        .metrics = metrics,
      };
    }
  } // namespace

  void PullMetrics::writeText(std::ostream& out) const
  {
    out << "Pull summary\n\nDiscovery:\n";
    printMetric(out, "Tables discovered", tablesDiscovered);
    printMetric(out, "Tables selected", tablesSelected);
    out << "\nComparison:\n";
    printMetric(out, "Existing entities", existingEntities);
    printMetric(out, "Compatible entities", compatibleEntities);
    printMetric(out, "Incompatible entities", incompatibleEntities);
    printMetric(out, "Missing entities", missingEntities);
    out << "\nPlan:\n";
    printMetric(out, "Planned entities", plannedEntities);
    printMetric(out, "Generated entities", generatedEntities);
    printMetric(out, "Failed entities", failedEntities);
    out << "\nTiming:\n";
    printDuration(out, "Discovery", discoveryDuration);
    printDuration(out, "Comparison", comparisonDuration);
    printDuration(out, "Planning", planningDuration);
    printDuration(out, "Execution", executionDuration);
    printDuration(out, "Total", totalDuration);
  }

  void PullMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"tablesDiscovered\":" << tablesDiscovered << ",\"tablesSelected\":" << tablesSelected
        << ",\"existingEntities\":" << existingEntities << ",\"compatibleEntities\":" << compatibleEntities
        << ",\"incompatibleEntities\":" << incompatibleEntities << ",\"missingEntities\":" << missingEntities
        << ",\"plannedEntities\":" << plannedEntities << ",\"generatedEntities\":" << generatedEntities
        << ",\"failedEntities\":" << failedEntities << '}';
  }

  ExecutionReport pull(const Invocation& invocation, const core::SchemaSnapshot& databaseSchema)
  {
    const auto started = Clock::now();
    auto metrics = std::make_shared<PullMetrics>();
    ExecutionReport report = createFiles(invocation, databaseSchema, metrics);
    metrics->totalDuration = Clock::now() - started;
    return report;
  }

  ExecutionReport pull(const Invocation& invocation)
  {
    const auto started = Clock::now();
    auto inspector = schemaInspector(invocation);
    const auto databaseSchema = inspector.inspect();
    const auto discoveryFinished = Clock::now();
    auto metrics = std::make_shared<PullMetrics>();
    ExecutionReport report = createFiles(invocation, databaseSchema, metrics);
    metrics->discoveryDuration = discoveryFinished - started;
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::generator
