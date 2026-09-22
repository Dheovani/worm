#pragma once

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

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

  [[nodiscard]]
  constexpr std::string_view getClassName(std::string_view path) noexcept
  {
    const std::size_t separator = path.find_last_of("/\\");
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
  }

  [[nodiscard]]
  constexpr std::string_view getLogTypeMessage(LogLevel level) noexcept
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
    class Message
    {
    public:
      constexpr Message(const char* text, std::source_location location = std::source_location::current()) noexcept
        : text_(text),
          location_(location)
      {}

      [[nodiscard]]
      constexpr const char* text() const noexcept
      {
        return text_;
      }

      [[nodiscard]]
      constexpr const std::source_location& location() const noexcept
      {
        return location_;
      }

    private:
      const char* text_;
      std::source_location location_;
    };

    class StreamValue
    {
    public:
      template <typename Type>
      StreamValue(const Type& value, std::source_location location = std::source_location::current())
        : location_(location)
      {
        std::ostringstream stream;
        stream << value;
        text_ = stream.str();
      }

      [[nodiscard]]
      const std::string& text() const noexcept
      {
        return text_;
      }

      [[nodiscard]]
      const std::source_location& location() const noexcept
      {
        return location_;
      }

    private:
      std::string text_;
      std::source_location location_;
    };

    constexpr Logger() noexcept = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    const Logger& operator<<(StreamValue value) const
    {
      logAt(LogLevel::Debug, value.location(), "%s", value.text().c_str());
      return *this;
    }

    template <typename... Args>
    void info(Message message, Args&&... args) const
    {
      logAt(LogLevel::Info, message.location(), message.text(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(Message message, Args&&... args) const
    {
      logAt(LogLevel::Debug, message.location(), message.text(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace(Message message, Args&&... args) const
    {
      logAt(LogLevel::Trace, message.location(), message.text(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(Message message, Args&&... args) const
    {
      logAt(LogLevel::Warning, message.location(), message.text(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(Message message, Args&&... args) const
    {
      logAt(LogLevel::Error, message.location(), message.text(), std::forward<Args>(args)...);
    }

    void error(const std::exception& exception, std::source_location location = std::source_location::current()) const
    {
      logAt(LogLevel::Error, location, "%s", exception.what());
    }

  private:
    template <typename... Args>
    void logAt(LogLevel level, const std::source_location& location, const char* message, Args&&... args) const
    {
      const std::string formattedMessage = format(message, std::forward<Args>(args)...);
      const std::string_view className = getClassName(location.file_name());
      const std::string_view logType = getLogTypeMessage(level);

      std::printf(
        "%.*s, line: %u %.*s%s\n",
        static_cast<int>(className.size()),
        className.data(),
        static_cast<unsigned>(location.line()),
        static_cast<int>(logType.size()),
        logType.data(),
        formattedMessage.c_str());

#ifdef _LOG_FILE
      std::string filename(className);
      filename += "_log_file.log";

      std::ofstream file(filename, std::ios::app);
      if (file.is_open())
        file << logType << formattedMessage << '\n';
      else
        std::cerr << "Failed opening log file.\n";
#endif
    }

    template <typename... Args>
    [[nodiscard]]
    static std::string format(const char* message, Args&&... args)
    {
      const int size = std::snprintf(nullptr, 0, message, std::forward<Args>(args)...);
      if (size <= 0)
        return message;

      std::string result(static_cast<std::size_t>(size), '\0');
      std::snprintf(result.data(), result.size() + 1, message, std::forward<Args>(args)...);

      return result;
    }
  };

  inline constexpr Logger logger{};
} // namespace worm
