#pragma once

#include <exception>
#include <string>
#include <utility>

namespace worm
{
  class DatabaseException : public std::exception
  {
  public:
    explicit DatabaseException(std::string message) : message_(std::move(message)) {}

    [[nodiscard]] const char* what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    std::string message_;
  };
} // namespace worm
