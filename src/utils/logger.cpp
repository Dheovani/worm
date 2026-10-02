#include <utils/logger.hpp>

#include <iostream>

namespace worm
{
  namespace
  {
    void writeFieldValue(std::ostream& output, std::string_view value)
    {
      output << '"';
      for (const char character : value) {
        switch (character) {
        case '\\':
          output << "\\\\";
          break;
        case '"':
          output << "\\\"";
          break;
        case '\n':
          output << "\\n";
          break;
        case '\r':
          output << "\\r";
          break;
        case '\t':
          output << "\\t";
          break;
        default:
          output << character;
        }
      }
      output << '"';
    }
  } // namespace

  Logger logger{std::clog};

  Logger::Logger(std::ostream& output, LogLevel minimumLevel) noexcept
    : output_(&output),
      minimumLevel_(minimumLevel)
  {}

  void Logger::setMinimumLevel(LogLevel level) noexcept
  {
    minimumLevel_.store(level, std::memory_order_relaxed);
  }

  LogLevel Logger::minimumLevel() const noexcept
  {
    return minimumLevel_.load(std::memory_order_relaxed);
  }

  bool Logger::enabled(LogLevel level) const noexcept
  {
    return level != LogLevel::Off && level >= minimumLevel();
  }

  void Logger::log(LogLevel level, Message message, std::initializer_list<LogField> fields) const
  {
    if (!enabled(level)) {
      return;
    }

    write(level, message.location(), message.text(), fields);
  }

  void Logger::error(const std::exception& exception, std::source_location location) const
  {
    if (!enabled(LogLevel::Error)) {
      return;
    }

    write(LogLevel::Error, location, exception.what(), {});
  }

  void Logger::write(
    LogLevel level,
    const std::source_location& location,
    std::string_view message,
    std::initializer_list<LogField> fields) const
  {
    const std::scoped_lock lock{mutex_};
    *output_ << getLogTypeMessage(level) << getClassName(location.file_name()) << ':' << location.line() << ' '
             << message;

    for (const LogField& field : fields) {
      *output_ << ' ' << field.name << '=';
      writeFieldValue(*output_, field.value);
    }

    *output_ << '\n';
    output_->flush();
  }
} // namespace worm
