#include <utils/sensitive-data.hpp>

#include <iostream>
#include <string>

int main()
{
  if (!worm::utils::isSensitiveName("password") || !worm::utils::isSensitiveName("database_password") ||
      !worm::utils::isSensitiveName("ACCESS-TOKEN") || !worm::utils::isSensitiveName("connection_string") ||
      worm::utils::isSensitiveName("token_count") || worm::utils::isSensitiveName("database")) {
    std::cerr << "Sensitive structured field classification is invalid.\n";
    return 1;
  }

  const std::string redacted = worm::utils::redactSensitiveText(
    "password=alpha-secret PWD={beta;secret} \"access_token\":\"gamma-secret\" "
    "postgresql://worm:delta-secret@localhost/worm authorization Bearer-secret");
  if (redacted.find("alpha-secret") != std::string::npos || redacted.find("beta;secret") != std::string::npos ||
      redacted.find("gamma-secret") != std::string::npos || redacted.find("delta-secret") != std::string::npos ||
      redacted.find("Bearer-secret") != std::string::npos || redacted.find("localhost/worm") == std::string::npos ||
      redacted.find(worm::utils::redactedValue) == std::string::npos) {
    std::cerr << "Sensitive text redaction exposed a credential or removed safe connection context.\n";
    return 1;
  }

  const std::string placeholders = "secret = ? password=$1 access_token=:token";
  const std::string redactedPlaceholders = worm::utils::redactSensitiveText(placeholders);
  if (redactedPlaceholders != placeholders) {
    std::cerr << "Sensitive text redaction modified safe SQL placeholders: " << redactedPlaceholders << '\n';
    return 1;
  }

  const std::string nativeError =
    worm::utils::redactSensitiveValue("authentication failed for opaque-native-secret", "opaque-native-secret");
  if (nativeError.find("opaque-native-secret") != std::string::npos ||
      nativeError.find(worm::utils::redactedValue) == std::string::npos ||
      worm::utils::redactSensitiveValue("unchanged", "") != "unchanged") {
    std::cerr << "Known sensitive value redaction is invalid.\n";
    return 1;
  }

  return 0;
}
