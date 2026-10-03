#pragma once

#include <string>
#include <string_view>

namespace worm::utils
{
  inline constexpr std::string_view redactedValue{"<redacted>"};

  [[nodiscard]]
  bool isSensitiveName(std::string_view name);

  [[nodiscard]]
  std::string redactSensitiveText(std::string_view text);

  [[nodiscard]]
  std::string redactSensitiveValue(std::string_view text, std::string_view sensitiveValue);
} // namespace worm::utils
