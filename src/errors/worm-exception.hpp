#pragma once

#include <exception>
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

    [[nodiscard]]
    const char* what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };
} // namespace worm
