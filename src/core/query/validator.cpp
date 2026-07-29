#include <core/query/validator.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace worm::core
{

  namespace
  {
    [[nodiscard]]
    std::string extractFirstWord(const std::string& query) noexcept
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
    const std::string firstWord = extractFirstWord(query);
    return firstWord == insertKeyword;
  }

  bool isUpdate(const std::string& query) noexcept
  {
    const std::string firstWord = extractFirstWord(query);
    return firstWord == updateKeyword;
  }

  bool isDelete(const std::string& query) noexcept
  {
    const std::string firstWord = extractFirstWord(query);
    return firstWord == deleteKeyword;
  }

  bool isSelect(const std::string& query) noexcept
  {
    const std::string firstWord = extractFirstWord(query);
    return firstWord == selectKeyword;
  }

} // namespace worm::core
