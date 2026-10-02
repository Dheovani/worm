#pragma once

#include <atomic>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <mutex>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace worm
{
  enum class LogLevel
  {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Off
  };

  struct LogField
  {
    std::string_view name;
    std::string value;
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
    case LogLevel::Trace:
      return "[TRACE] ";
    case LogLevel::Debug:
      return "[DEBUG] ";
    case LogLevel::Info:
      return "[INFO] ";
    case LogLevel::Warning:
      return "[WARNING] ";
    case LogLevel::Error:
      return "[ERROR] ";
    case LogLevel::Off:
      return "[OFF] ";
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

    explicit Logger(std::ostream& output, LogLevel minimumLevel = LogLevel::Warning) noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void setMinimumLevel(LogLevel level) noexcept;

    [[nodiscard]]
    LogLevel minimumLevel() const noexcept;

    [[nodiscard]]
    bool enabled(LogLevel level) const noexcept;

    void log(LogLevel level, Message message, std::initializer_list<LogField> fields = {}) const;

    template <typename... Args>
    void info(Message message, Args&&... args) const
    {
      logFormatted(LogLevel::Info, message, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(Message message, Args&&... args) const
    {
      logFormatted(LogLevel::Debug, message, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace(Message message, Args&&... args) const
    {
      logFormatted(LogLevel::Trace, message, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(Message message, Args&&... args) const
    {
      logFormatted(LogLevel::Warning, message, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(Message message, Args&&... args) const
    {
      logFormatted(LogLevel::Error, message, std::forward<Args>(args)...);
    }

    void error(const std::exception& exception, std::source_location location = std::source_location::current()) const;

  private:
    template <typename... Args>
    void logFormatted(LogLevel level, Message message, Args&&... args) const
    {
      if (!enabled(level)) {
        return;
      }

      write(level, message.location(), format(message.text(), std::forward<Args>(args)...), {});
    }

    void write(
      LogLevel level,
      const std::source_location& location,
      std::string_view message,
      std::initializer_list<LogField> fields) const;

    template <typename... Args>
    [[nodiscard]]
    static std::string format(const char* message, Args&&... args)
    {
      if constexpr (sizeof...(Args) == 0) {
        return message;
      } else {
        const int size = std::snprintf(nullptr, 0, message, std::forward<Args>(args)...);
        if (size <= 0) {
          return message;
        }

        std::string result(static_cast<std::size_t>(size), '\0');
        std::snprintf(result.data(), result.size() + 1, message, std::forward<Args>(args)...);
        return result;
      }
    }

    std::ostream* output_;
    std::atomic<LogLevel> minimumLevel_;
    mutable std::mutex mutex_;
  };

  extern Logger logger;
} // namespace worm
