#pragma once

#include <string>
#include <string_view>

namespace worm::core
{

  enum class Operation
  {
    Insert,
    Update,
    Delete,
    Select
  };

  constexpr std::string_view insert_keyword = "INSERT";
  constexpr std::string_view update_keyword = "UPDATE";
  constexpr std::string_view delete_keyword = "DELETE";
  constexpr std::string_view select_keyword = "SELECT";

  [[nodiscard]]
  bool isInsert(const std::string& query) noexcept;

  [[nodiscard]]
  bool isUpdate(const std::string& query) noexcept;

  [[nodiscard]]
  bool isDelete(const std::string& query) noexcept;

  [[nodiscard]]
  bool isSelect(const std::string& query) noexcept;

} // namespace worm::core
