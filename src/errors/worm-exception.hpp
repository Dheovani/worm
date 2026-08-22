#pragma once

#include <exception>
#include <format>
#include <string>
#include <utility>

namespace worm
{
  class WormException : public std::exception
  {
  public:
    explicit WormException(std::string message)
      : message_(std::move(message))
    {}

    template <typename... Args>
    explicit WormException(std::format_string<Args...> format, Args&&... args)
      : message_(std::format(format, std::forward<Args>(args)...))
    {}

    [[nodiscard]]
    const char* what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };
} // namespace worm
