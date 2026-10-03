#include <utils/logger.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

int main()
{
  static_assert(!std::is_copy_constructible_v<worm::Logger>);
  static_assert(!std::is_copy_assignable_v<worm::Logger>);

  if (worm::getClassName("C:\\project\\entity.cpp") != "entity.cpp" ||
      worm::getClassName("/project/entity.cpp") != "entity.cpp" || worm::getClassName("entity.cpp") != "entity.cpp") {
    std::cerr << "getClassName returned an unexpected value.\n";
    return 1;
  }

  if (worm::getLogTypeMessage(worm::LogLevel::Info) != "[INFO] " ||
      worm::getLogTypeMessage(worm::LogLevel::Debug) != "[DEBUG] " ||
      worm::getLogTypeMessage(worm::LogLevel::Trace) != "[TRACE] " ||
      worm::getLogTypeMessage(worm::LogLevel::Warning) != "[WARNING] " ||
      worm::getLogTypeMessage(worm::LogLevel::Error) != "[ERROR] " ||
      worm::getLogTypeMessage(worm::LogLevel::Off) != "[OFF] ") {
    std::cerr << "getLogTypeMessage returned an unexpected value.\n";
    return 1;
  }

  std::ostringstream output;
  worm::Logger logger{output, worm::LogLevel::Info};
  logger.debug("Hidden diagnostic");
  logger.info("Logger smoke test: %s", "ok");
  logger.log(
    worm::LogLevel::Warning,
    "Structured diagnostic",
    {
      {"driver", "sqlite"},
      {"detail", "quoted \"value\"\nnext line"},
      {"password", "structured-secret"},
      {"reason", "connection failed with password=native-secret"},
      {"token_count", "3"},
    });
  logger.error(std::runtime_error{"postgresql://worm:uri-secret@localhost/worm"});

  const std::string contents = output.str();
  const bool hasCallSite = contents.find("[INFO] logger-tests.cpp:") != std::string::npos;
#if defined(__apple_build_version__)
  // AppleClang may report the implicit Message conversion in the header instead of at its call site.
  const bool hasPortableSourceLocation = hasCallSite || contents.find("[INFO] logger.hpp:") != std::string::npos;
#else
  const bool hasPortableSourceLocation = hasCallSite;
#endif
  if (contents.find("Hidden diagnostic") != std::string::npos ||
      !hasPortableSourceLocation ||
      contents.find("Logger smoke test: ok") == std::string::npos ||
      contents.find("driver=\"sqlite\"") == std::string::npos ||
      contents.find("detail=\"quoted \\\"value\\\"\\nnext line\"") == std::string::npos ||
      contents.find("password=\"<redacted>\"") == std::string::npos ||
      contents.find("reason=\"connection failed with password=<redacted>\"") == std::string::npos ||
      contents.find("token_count=\"3\"") == std::string::npos ||
      contents.find("structured-secret") != std::string::npos || contents.find("native-secret") != std::string::npos ||
      contents.find("uri-secret") != std::string::npos) {
    std::cerr << "Logger filtering or structured output is invalid:\n" << contents;
    return 1;
  }

  logger.setMinimumLevel(worm::LogLevel::Off);
  const std::size_t previousSize = output.str().size();
  logger.error("Disabled diagnostic");
  if (logger.minimumLevel() != worm::LogLevel::Off || output.str().size() != previousSize) {
    std::cerr << "Logger could not be disabled.\n";
    return 1;
  }

  return 0;
}
