#pragma once

#include <string_view>

namespace worm::core
{
  namespace detail
  {
    [[nodiscard]]
    constexpr bool isSqlSpace(const char character) noexcept
    {
      return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' ||
             character == '\v';
    }

    [[nodiscard]]
    constexpr char toSqlUpper(const char character) noexcept
    {
      if (character >= 'a' && character <= 'z') {
        return static_cast<char>(character - ('a' - 'A'));
      }

      return character;
    }

    [[nodiscard]]
    constexpr bool equalsKeyword(std::string_view left, std::string_view right) noexcept
    {
      if (left.size() != right.size()) {
        return false;
      }

      for (std::size_t index = 0; index < left.size(); ++index) {
        if (toSqlUpper(left[index]) != right[index]) {
          return false;
        }
      }

      return true;
    }

    [[nodiscard]]
    constexpr std::string_view extractFirstWord(std::string_view query) noexcept
    {
      std::size_t begin = 0;

      while (begin < query.size() && isSqlSpace(query[begin])) {
        ++begin;
      }

      std::size_t end = begin;
      while (end < query.size() && !isSqlSpace(query[end])) {
        ++end;
      }

      return query.substr(begin, end - begin);
    }
  } // namespace detail

  enum class Operation
  {
    Insert,
    Update,
    Delete,
    Select
  };

  inline constexpr std::string_view insertKeyword = "INSERT";
  inline constexpr std::string_view updateKeyword = "UPDATE";
  inline constexpr std::string_view deleteKeyword = "DELETE";
  inline constexpr std::string_view selectKeyword = "SELECT";

  [[nodiscard]]
  constexpr bool isInsert(std::string_view query) noexcept
  {
    return detail::equalsKeyword(detail::extractFirstWord(query), insertKeyword);
  }

  [[nodiscard]]
  constexpr bool isUpdate(std::string_view query) noexcept
  {
    return detail::equalsKeyword(detail::extractFirstWord(query), updateKeyword);
  }

  [[nodiscard]]
  constexpr bool isDelete(std::string_view query) noexcept
  {
    return detail::equalsKeyword(detail::extractFirstWord(query), deleteKeyword);
  }

  [[nodiscard]]
  constexpr bool isSelect(std::string_view query) noexcept
  {
    return detail::equalsKeyword(detail::extractFirstWord(query), selectKeyword);
  }

  [[nodiscard]]
  constexpr bool isSafeDdlExpression(std::string_view expression) noexcept
  {
    if (expression.empty() || expression.find(';') != std::string_view::npos ||
        expression.find("--") != std::string_view::npos || expression.find("/*") != std::string_view::npos ||
        expression.find("*/") != std::string_view::npos || expression.find('#') != std::string_view::npos ||
        expression.find('\0') != std::string_view::npos) {
      return false;
    }

    bool containsToken = false;
    for (const char character : expression) {
      if (character == '\n' || character == '\r') {
        return false;
      }

      if (character != ' ' && character != '\t' && character != '\f' && character != '\v') {
        containsToken = true;
      }
    }

    return containsToken;
  }
} // namespace worm::core
