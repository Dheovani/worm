#pragma once

#include <string>

namespace worm::core
{

  enum class Operation
  {
    Insert,
    Update,
    Delete,
    Select
  };

  inline constexpr const char* insertKeyword = "INSERT";
  inline constexpr const char* updateKeyword = "UPDATE";
  inline constexpr const char* deleteKeyword = "DELETE";
  inline constexpr const char* selectKeyword = "SELECT";

  [[nodiscard]]
  bool isInsert(const std::string& query) noexcept;

  [[nodiscard]]
  bool isUpdate(const std::string& query) noexcept;

  [[nodiscard]]
  bool isDelete(const std::string& query) noexcept;

  [[nodiscard]]
  bool isSelect(const std::string& query) noexcept;

} // namespace worm::core
