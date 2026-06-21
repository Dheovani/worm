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
      worm::getClassName("/project/entity.cpp") != "entity.cpp" ||
      worm::getClassName("entity.cpp") != "entity.cpp") {
    std::cerr << "getClassName returned an unexpected value.\n";
    return 1;
  }

  if (worm::getLogTypeMessage(worm::LogLevel::Info) != "[INFO] " ||
      worm::getLogTypeMessage(worm::LogLevel::Debug) != "[DEBUG] " ||
      worm::getLogTypeMessage(worm::LogLevel::Trace) != "[TRACE] " ||
      worm::getLogTypeMessage(worm::LogLevel::Warning) != "[WARNING] " ||
      worm::getLogTypeMessage(worm::LogLevel::Error) != "[ERROR] ") {
    std::cerr << "getLogTypeMessage returned an unexpected value.\n";
    return 1;
  }

  worm::Logger logger("logger-tests.cpp", 42);
  logger.info("Logger smoke test: %s", "ok");

  return 0;
}
