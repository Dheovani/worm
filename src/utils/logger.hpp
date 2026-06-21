#pragma once

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace worm
{
  enum class LogLevel
  {
    Info,
    Debug,
    Trace,
    Warning,
    Error
  };

  [[nodiscard]] inline std::string getClassName(const char* path)
  {
    const std::string fullPath(path);
    const std::size_t separator = fullPath.find_last_of("/\\");
    return separator == std::string::npos ? fullPath : fullPath.substr(separator + 1);
  }

  [[nodiscard]] inline std::string getLogTypeMessage(LogLevel level)
  {
    switch (level) {
    case LogLevel::Info:
      return "[INFO] ";
    case LogLevel::Debug:
      return "[DEBUG] ";
    case LogLevel::Trace:
      return "[TRACE] ";
    case LogLevel::Warning:
      return "[WARNING] ";
    case LogLevel::Error:
      return "[ERROR] ";
    }

    return "[UNKNOWN] ";
  }

  class Logger
  {
  public:
    template <typename Class> Logger(Class source, int lineNumber) : line_(lineNumber)
    {
      if constexpr (std::is_convertible_v<Class, const char*>)
        className_ = getClassName(source);
      else
        className_ = typeid(source).name();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template <typename Type> const Logger& operator<<(const Type& value) const
    {
      std::ostringstream stream;
      stream << value;
      debug("%s", stream.str().c_str());
      return *this;
    }

    template <typename... Args> void log(LogLevel level, const char* message, Args... args) const
    {
      std::ostringstream prefix;
      prefix << className_;
      if (line_ != 0)
        prefix << ", line: " << line_;
      prefix << ' ' << getLogTypeMessage(level);

      std::printf("%s", prefix.str().c_str());
      std::printf(message, args...);
      std::printf("\n");

#ifdef _LOG_FILE
      const std::string filename = className_ + "_log_file.log";
      std::ofstream file(filename, std::ios::app);
      if (file.is_open())
        file << getLogTypeMessage(level) << message << '\n';
      else
        std::cerr << "Failed opening log file.\n";
#endif
    }

    template <typename... Args> void info(const char* message, Args... args) const
    {
      log(LogLevel::Info, message, args...);
    }

    template <typename... Args> void debug(const char* message, Args... args) const
    {
      log(LogLevel::Debug, message, args...);
    }

    template <typename... Args> void trace(const char* message, Args... args) const
    {
      log(LogLevel::Trace, message, args...);
    }

    template <typename... Args> void warning(const char* message, Args... args) const
    {
      log(LogLevel::Warning, message, args...);
    }

    template <typename... Args> void error(const char* message, Args... args) const
    {
      log(LogLevel::Error, message, args...);
    }

    void error(const std::exception& exception) const
    {
      log(LogLevel::Error, "%s", exception.what());
    }

  private:
    int line_;
    std::string className_;
  };
} // namespace worm

#define WORM_LOGGER worm::Logger(__FILE__, __LINE__)
