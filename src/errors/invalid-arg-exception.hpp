#pragma once

#include <exception>
#include <string>
#include <utility>

namespace worm
{
  class InvalidArgException : public std::exception
  {
  public:
    explicit InvalidArgException(std::string message) : message_(std::move(message)) {}

    [[nodiscard]] const char* what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };
} // namespace worm
