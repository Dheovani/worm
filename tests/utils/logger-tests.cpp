#include <utils/logger.hpp>

#include <exception>
#include <iostream>
#include <sstream>
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
    });

  const std::string contents = output.str();
  if (contents.find("Hidden diagnostic") != std::string::npos ||
      contents.find("[INFO] logger-tests.cpp:") == std::string::npos ||
      contents.find("Logger smoke test: ok") == std::string::npos ||
      contents.find("driver=\"sqlite\"") == std::string::npos ||
      contents.find("detail=\"quoted \\\"value\\\"\\nnext line\"") == std::string::npos) {
    std::cerr << "Logger filtering or structured output is invalid.\n";
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
