#pragma once

#include <core/query/statement.hpp>
#include <core/query/validator.hpp>
#include <errors/invalid-arg-exception.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace worm::utils
{
  struct NPlusOneWarning
  {
    std::string sql;
    std::size_t executions{};
    std::size_t distinctParameterSets{};
  };

  class NPlusOneDetector final
  {
  public:
    explicit NPlusOneDetector(std::size_t minimumExecutions = 2)
      : minimumExecutions_(minimumExecutions)
    {
      if (minimumExecutions_ < 2) {
        throw InvalidArgException("N+1 detection requires at least two executions.");
      }
    }

    void record(const core::Statement& statement)
    {
      ++executionCount_;

      if (!core::isSelect(statement.sql) || statement.parameters.empty()) {
        return;
      }

      auto entry = std::find_if(
        entries_.begin(), entries_.end(), [&](const Entry& candidate) { return candidate.sql == statement.sql; });

      if (entry == entries_.end()) {
        entries_.push_back(Entry{statement.sql, 1, {statement.parameters}});
        return;
      }

      ++entry->executions;
      if (!hasParameterSet(*entry, statement.parameters)) {
        entry->parameterSets.push_back(statement.parameters);
      }
    }

    [[nodiscard]]
    std::vector<NPlusOneWarning> warnings() const
    {
      std::vector<NPlusOneWarning> result;

      for (const Entry& entry : entries_) {
        if (entry.executions >= minimumExecutions_ && entry.parameterSets.size() >= minimumExecutions_) {
          result.push_back(NPlusOneWarning{
            entry.sql,
            entry.executions,
            entry.parameterSets.size(),
          });
        }
      }

      return result;
    }

    [[nodiscard]]
    std::size_t executionCount() const noexcept
    {
      return executionCount_;
    }

    void clear() noexcept
    {
      entries_.clear();
      executionCount_ = 0;
    }

  private:
    struct Entry
    {
      std::string sql;
      std::size_t executions{};
      std::vector<std::vector<core::Parameter>> parameterSets;
    };

    [[nodiscard]]
    static bool hasParameterSet(const Entry& entry, const std::vector<core::Parameter>& parameters)
    {
      return std::find(entry.parameterSets.begin(), entry.parameterSets.end(), parameters) != entry.parameterSets.end();
    }

    std::size_t minimumExecutions_;
    std::vector<Entry> entries_;
    std::size_t executionCount_{};
  };
} // namespace worm::utils
