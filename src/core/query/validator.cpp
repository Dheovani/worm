#include <core/query/validator.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace worm::core
{

  namespace
  {
    [[nodiscard]]
    std::string extractFistWord(const std::string& query) noexcept
    {
      std::istringstream iss(query);
      std::string firstWord;

      iss >> firstWord;
      std::transform(firstWord.begin(), firstWord.end(), firstWord.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
      });

      return firstWord;
    }
  } // namespace

  bool isInsert(const std::string& query) noexcept
  {
    const std::string firstWord = extractFistWord(query);
    return firstWord == insert_keyword;
  }

  bool isUpdate(const std::string& query) noexcept
  {
    const std::string firstWord = extractFistWord(query);
    return firstWord == update_keyword;
  }

  bool isDelete(const std::string& query) noexcept
  {
    const std::string firstWord = extractFistWord(query);
    return firstWord == delete_keyword;
  }

  bool isSelect(const std::string& query) noexcept
  {
    const std::string firstWord = extractFistWord(query);
    return firstWord == select_keyword;
  }

} // namespace worm::core
