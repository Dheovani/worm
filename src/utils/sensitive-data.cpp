#include <utils/sensitive-data.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <string>
#include <utility>

namespace worm::utils
{
  namespace
  {
    [[nodiscard]]
    std::string normalizedName(std::string_view name)
    {
      std::string result;
      result.reserve(name.size());
      for (const char character : name) {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
          result += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        } else if (!result.empty() && result.back() != '_') {
          result += '_';
        }
      }
      return result;
    }

    [[nodiscard]]
    bool hasSensitiveSuffix(std::string_view name, std::string_view suffix) noexcept
    {
      return name == suffix ||
             (name.size() > suffix.size() && name.ends_with(suffix) && name[name.size() - suffix.size() - 1] == '_');
    }

    [[nodiscard]]
    bool isSqlPlaceholder(std::string_view value) noexcept
    {
      if (value == "?") {
        return true;
      }
      if (value.size() < 2 || (value.front() != '$' && value.front() != ':' && value.front() != '@')) {
        return false;
      }

      return std::ranges::all_of(value.substr(1), [prefix = value.front()](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return prefix == '$' ? std::isdigit(byte) != 0 : std::isalnum(byte) != 0 || character == '_';
      });
    }

    [[nodiscard]]
    std::string redactMatches(std::string text, const std::regex& pattern)
    {
      std::string result;
      std::smatch match;
      while (std::regex_search(text, match, pattern)) {
        result.append(match.prefix().first, match.prefix().second);
        result.append(match[1].first, match[1].second);
        const std::string matchedValue = match[2].str();
        result +=
          isSqlPlaceholder(matchedValue) || matchedValue == "=" || matchedValue == ":" ? matchedValue : redactedValue;
        text.assign(match.suffix().first, match.suffix().second);
      }
      result += text;
      return result;
    }
  } // namespace

  bool isSensitiveName(std::string_view name)
  {
    const std::string normalized = normalizedName(name);
    static constexpr std::array exactNames{
      std::string_view{"authorization"},
      std::string_view{"cookie"},
      std::string_view{"set_cookie"},
      std::string_view{"dsn"},
      std::string_view{"connection_string"},
      std::string_view{"private_key"},
    };

    if (std::ranges::find(exactNames, normalized) != exactNames.end()) {
      return true;
    }

    return hasSensitiveSuffix(normalized, "password") || hasSensitiveSuffix(normalized, "passwd") ||
           hasSensitiveSuffix(normalized, "pwd") || hasSensitiveSuffix(normalized, "secret") ||
           hasSensitiveSuffix(normalized, "token") || hasSensitiveSuffix(normalized, "credential");
  }

  std::string redactSensitiveText(std::string_view text)
  {
    static const std::regex assignedSecret{
      R"(((?:[A-Za-z0-9_-]*[_-])?(?:password|passwd|pwd|secret|token|authorization|credential|private_key|connection_string|dsn)\s*["']?\s*[:=]\s*)(\{[^}]*\}|'[^']*'|"[^"]*"|[^\s;,]+))",
      std::regex::icase};
    static const std::regex argumentSecret{
      R"(((?:--)?(?:[A-Za-z0-9_-]*[_-])?(?:password|passwd|pwd|secret|token|authorization|credential)\s+)(\{[^}]*\}|'[^']*'|"[^"]*"|[^\s;,]+))",
      std::regex::icase};
    static const std::regex uriCredential{R"(([A-Za-z][A-Za-z0-9+.-]*://[^:/@\s]+:)([^@\s]*)(@))"};

    std::string result = redactMatches(std::string{text}, assignedSecret);
    result = redactMatches(std::move(result), argumentSecret);
    return std::regex_replace(result, uriCredential, "$1<redacted>$3");
  }

  std::string redactSensitiveValue(std::string_view text, std::string_view sensitiveValue)
  {
    std::string result{text};
    if (sensitiveValue.empty()) {
      return result;
    }

    std::size_t position = 0;
    while ((position = result.find(sensitiveValue, position)) != std::string::npos) {
      result.replace(position, sensitiveValue.size(), redactedValue);
      position += redactedValue.size();
    }
    return result;
  }
} // namespace worm::utils
