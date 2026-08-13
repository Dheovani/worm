#pragma once

#include <core/query/expression.hpp>
#include <core/query/statement.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace worm::core
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
    explicit NPlusOneDetector(std::size_t minimumExecutions = 2);

    void record(const Statement& statement);

    [[nodiscard]]
    std::vector<NPlusOneWarning> warnings() const;

    [[nodiscard]]
    std::size_t executionCount() const noexcept;

    void clear() noexcept;

  private:
    struct Entry
    {
      std::string sql;
      std::size_t executions{};
      std::vector<std::vector<Parameter>> parameterSets;
    };

    [[nodiscard]]
    static bool hasParameterSet(const Entry& entry, const std::vector<Parameter>& parameters);

    std::size_t minimumExecutions_;
    std::vector<Entry> entries_;
    std::size_t executionCount_{};
  };

} // namespace worm::core
