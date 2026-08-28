#include "n-plus-one.hpp"

#include <core/query/statement.hpp>
#include <utils/n-plus-one-detector.hpp>

#include <cctype>
#include <charconv>
#include <chrono>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "../errors/invalid-cli-argument-exception.hpp"
#include "../helpers/file.hpp"

namespace worm::cli::database
{
  namespace
  {
    using Clock = std::chrono::steady_clock;

    [[nodiscard]]
    bool equalsCaseInsensitive(std::string_view left, std::string_view right) noexcept
    {
      if (left.size() != right.size())
        return false;

      for (std::size_t index = 0; index < left.size(); ++index) {
        const auto lhs = static_cast<unsigned char>(left[index]);
        const auto rhs = static_cast<unsigned char>(right[index]);
        if (std::tolower(lhs) != std::tolower(rhs))
          return false;
      }

      return true;
    }

    void appendSpace(std::string& sql)
    {
      if (!sql.empty() && sql.back() != ' ')
        sql += ' ';
    }

    void appendPlaceholder(core::Statement& statement, std::string parameter)
    {
      statement.sql += '?';
      statement.parameters.emplace_back(std::move(parameter));
    }

    [[nodiscard]]
    core::Statement normalizeObservedQuery(std::string_view query, std::size_t executionIndex)
    {
      core::Statement statement;
      std::size_t parameterIndex = 0;

      for (std::size_t index = 0; index < query.size();) {
        const char current = query[index];
        const auto currentByte = static_cast<unsigned char>(current);

        if (std::isspace(currentByte)) {
          appendSpace(statement.sql);
          ++index;
          continue;
        }

        if (current == '-' && index + 1 < query.size() && query[index + 1] == '-') {
          index += 2;
          while (index < query.size() && query[index] != '\n')
            ++index;
          appendSpace(statement.sql);
          continue;
        }

        if (current == '/' && index + 1 < query.size() && query[index + 1] == '*') {
          const auto end = query.find("*/", index + 2);
          if (end == std::string_view::npos)
            throw InvalidCliArgumentException("N+1 query contains an unterminated block comment.");
          index = end + 2;
          appendSpace(statement.sql);
          continue;
        }

        if (current == '\'') {
          std::string value;
          bool closed = false;
          ++index;

          while (index < query.size()) {
            if (query[index] != '\'') {
              value += query[index++];
              continue;
            }

            if (index + 1 < query.size() && query[index + 1] == '\'') {
              value += '\'';
              index += 2;
              continue;
            }

            ++index;
            closed = true;
            break;
          }

          if (!closed)
            throw InvalidCliArgumentException("N+1 query contains an unterminated string literal.");

          appendPlaceholder(statement, std::move(value));
          ++parameterIndex;
          continue;
        }

        if (current == '"' || current == '`' || current == '[') {
          const char closing = current == '[' ? ']' : current;
          statement.sql += current;
          ++index;

          while (index < query.size()) {
            statement.sql += query[index];
            if (query[index] != closing) {
              ++index;
              continue;
            }

            if (closing != ']' && index + 1 < query.size() && query[index + 1] == closing) {
              statement.sql += query[index + 1];
              index += 2;
              continue;
            }

            ++index;
            break;
          }
          continue;
        }

        const bool numberBoundary = index == 0 || (!std::isalnum(static_cast<unsigned char>(query[index - 1])) &&
                                                    query[index - 1] != '_' && query[index - 1] != '$');
        if (numberBoundary && std::isdigit(currentByte)) {
          const std::size_t begin = index++;
          while (index < query.size() && std::isdigit(static_cast<unsigned char>(query[index])))
            ++index;

          if (index < query.size() && query[index] == '.') {
            ++index;
            while (index < query.size() && std::isdigit(static_cast<unsigned char>(query[index])))
              ++index;
          }

          if (index < query.size() && (query[index] == 'e' || query[index] == 'E')) {
            const auto exponent = index;
            ++index;
            if (index < query.size() && (query[index] == '+' || query[index] == '-'))
              ++index;

            const auto exponentDigits = index;
            while (index < query.size() && std::isdigit(static_cast<unsigned char>(query[index])))
              ++index;

            if (index == exponentDigits)
              index = exponent;
          }
          appendPlaceholder(statement, std::string{query.substr(begin, index - begin)});
          ++parameterIndex;
          continue;
        }

        const bool positionalPlaceholder =
          current == '$' && index + 1 < query.size() && std::isdigit(static_cast<unsigned char>(query[index + 1]));
        const bool namedPlaceholder =
          current == ':' && index + 1 < query.size() && query[index + 1] != ':' &&
          (std::isalpha(static_cast<unsigned char>(query[index + 1])) || query[index + 1] == '_');
        if (current == '?' || positionalPlaceholder || namedPlaceholder) {
          if (positionalPlaceholder) {
            index += 2;
            while (index < query.size() && std::isdigit(static_cast<unsigned char>(query[index])))
              ++index;
          } else if (namedPlaceholder) {
            index += 2;
            while (
              index < query.size() && (std::isalnum(static_cast<unsigned char>(query[index])) || query[index] == '_')) {
              ++index;
            }
          } else {
            ++index;
          }

          appendPlaceholder(statement, std::to_string(executionIndex) + ":" + std::to_string(parameterIndex++));
          continue;
        }

        if (std::isalpha(currentByte) || current == '_') {
          const std::size_t begin = index++;
          while (index < query.size() && (std::isalnum(static_cast<unsigned char>(query[index])) ||
                                           query[index] == '_' || query[index] == '$')) {
            ++index;
          }

          const auto word = query.substr(begin, index - begin);
          if (equalsCaseInsensitive(word, "true") || equalsCaseInsensitive(word, "false") ||
              equalsCaseInsensitive(word, "null")) {
            appendPlaceholder(statement, std::string{word});
            ++parameterIndex;
          } else {
            for (const char character : word)
              statement.sql += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
          }
          continue;
        }

        statement.sql += static_cast<char>(std::tolower(currentByte));
        ++index;
      }

      while (!statement.sql.empty() && statement.sql.back() == ' ')
        statement.sql.pop_back();

      return statement;
    }

    [[nodiscard]]
    std::size_t maximumExecutions(const Invocation& invocation)
    {
      constexpr std::size_t defaultMaximum = 1;
      if (!invocation.arguments.maxExecutions.has_value())
        return defaultMaximum;

      std::size_t result{};
      const std::string_view value = *invocation.arguments.maxExecutions;
      const auto [ptr, error] = std::from_chars(value.data(), value.data() + value.size(), result);
      if (error != std::errc{} || ptr != value.data() + value.size() || result == 0 ||
          result == std::numeric_limits<std::size_t>::max()) {
        throw InvalidCliArgumentException("Option '--max-executions' must be a positive integer.");
      }
      return result;
    }

    [[nodiscard]]
    std::string reportInfo(const std::vector<utils::NPlusOneWarning>& warnings)
    {
      if (warnings.empty())
        return "No potential N+1 query patterns were found.";

      std::string info = "Potential N+1 query patterns detected:";
      for (const auto& warning : warnings) {
        info += "\n  ";
        info += warning.sql;
        info += " (executions: ";
        info += std::to_string(warning.executions);
        info += ", distinct parameter sets: ";
        info += std::to_string(warning.distinctParameterSets);
        info += ')';
      }
      return info;
    }

    [[nodiscard]]
    ExecutionReport analyze(
      const std::vector<std::string>& queries,
      const std::shared_ptr<NPlusOneMetrics>& metrics,
      std::size_t allowedExecutions)
    {
      metrics->queriesDiscovered = queries.size();

      const auto parsingStarted = Clock::now();
      std::vector<core::Statement> statements;
      statements.reserve(queries.size());
      for (std::size_t index = 0; index < queries.size(); ++index)
        statements.push_back(normalizeObservedQuery(queries[index], index));
      metrics->parsingDuration = Clock::now() - parsingStarted;

      const auto analysisStarted = Clock::now();
      utils::NPlusOneDetector detector{allowedExecutions + 1};
      for (const auto& statement : statements)
        detector.record(statement);

      const auto warnings = detector.warnings();
      metrics->queriesAnalyzed = detector.executionCount();
      metrics->queryPatterns = detector.patternCount();
      metrics->repeatedPatterns = detector.repeatedPatternCount();
      metrics->potentialNPlusOnePatterns = warnings.size();
      metrics->affectedQueries =
        std::accumulate(warnings.begin(), warnings.end(), std::size_t{}, [](std::size_t total, const auto& warning) {
          return total + warning.executions;
        });
      metrics->findings.reserve(warnings.size());
      for (const auto& warning : warnings)
        metrics->findings.push_back(warning.sql);
      metrics->analysisDuration = Clock::now() - analysisStarted;

      return {
        .info = reportInfo(warnings),
        .status = warnings.empty() ? ExecutionStatus::Success : ExecutionStatus::IssuesDetected,
        .metrics = metrics,
      };
    }
  } // namespace

  void NPlusOneMetrics::writeText(std::ostream& out) const
  {
    out << "N+1 analysis metrics\n";
    printMetric(out, "Queries discovered", queriesDiscovered);
    printMetric(out, "Queries analyzed", queriesAnalyzed);
    printMetric(out, "Parameterized patterns", queryPatterns);
    printMetric(out, "Repeated patterns", repeatedPatterns);
    printMetric(out, "Potential N+1 patterns", potentialNPlusOnePatterns);
    printMetric(out, "Affected queries", affectedQueries);
    printDuration(out, "Parsing", parsingDuration);
    printDuration(out, "Analysis", analysisDuration);
    printDuration(out, "Total", totalDuration);
  }

  void NPlusOneMetrics::writeJson(std::ostream& out) const
  {
    out << "{\"queriesDiscovered\":" << queriesDiscovered << ",\"queriesAnalyzed\":" << queriesAnalyzed
        << ",\"queryPatterns\":" << queryPatterns << ",\"repeatedPatterns\":" << repeatedPatterns
        << ",\"potentialNPlusOnePatterns\":" << potentialNPlusOnePatterns << ",\"affectedQueries\":" << affectedQueries
        << ",\"parsingMilliseconds\":" << milliseconds(parsingDuration)
        << ",\"analysisMilliseconds\":" << milliseconds(analysisDuration)
        << ",\"totalMilliseconds\":" << milliseconds(totalDuration) << ",\"findings\":[";

    for (std::size_t index = 0; index < findings.size(); ++index) {
      if (index != 0)
        out << ',';
      writeJsonString(out, findings[index]);
    }
    out << "]}";
  }

  ExecutionReport verify(const Invocation& invocation)
  {
    const auto started = Clock::now();
    const auto metrics = std::make_shared<NPlusOneMetrics>();
    std::vector<std::string> queries;

    if (invocation.arguments.query.has_value()) {
      queries.push_back(*invocation.arguments.query);
    } else if (invocation.arguments.file.has_value()) {
      queries = core::splitStatementQueries(readFile(*invocation.arguments.file));
    } else {
      throw InvalidCliArgumentException("No query source was provided for the 'n-plus-one' command.");
    }

    auto report = analyze(queries, metrics, maximumExecutions(invocation));
    metrics->totalDuration = Clock::now() - started;
    return report;
  }
} // namespace worm::cli::database
