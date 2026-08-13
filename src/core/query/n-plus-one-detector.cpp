#include <core/query/n-plus-one-detector.hpp>

#include <core/query/validator.hpp>
#include <errors/invalid-arg-exception.hpp>

#include <algorithm>
#include <utility>

namespace worm::core
{

  NPlusOneDetector::NPlusOneDetector(std::size_t minimumExecutions)
    : minimumExecutions_(minimumExecutions)
  {
    if (minimumExecutions_ < 2) {
      throw InvalidArgException("N+1 detection requires at least two executions.");
    }
  }

  void NPlusOneDetector::record(const Statement& statement)
  {
    ++executionCount_;

    if (!isSelect(statement.sql) || statement.parameters.empty()) {
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

  std::vector<NPlusOneWarning> NPlusOneDetector::warnings() const
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

  std::size_t NPlusOneDetector::executionCount() const noexcept
  {
    return executionCount_;
  }

  void NPlusOneDetector::clear() noexcept
  {
    entries_.clear();
    executionCount_ = 0;
  }

  bool NPlusOneDetector::hasParameterSet(const Entry& entry, const std::vector<Parameter>& parameters)
  {
    return std::find(entry.parameterSets.begin(), entry.parameterSets.end(), parameters) != entry.parameterSets.end();
  }

} // namespace worm::core
