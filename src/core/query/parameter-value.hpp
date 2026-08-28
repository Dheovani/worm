#pragma once

#include <errors/invalid-arg-type-exception.hpp>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace worm::core
{
  class Decimal
  {
  public:
    Decimal() = default;

    explicit Decimal(std::string value)
      : value_(std::move(value))
    {
      bool digitFound = false;
      bool decimalPointFound = false;

      for (std::size_t index = 0; index < value_.size(); ++index) {
        const char character = value_[index];
        if (character >= '0' && character <= '9') {
          digitFound = true;
          continue;
        }
        if ((character == '+' || character == '-') && index == 0) {
          continue;
        }
        if (character == '.' && !decimalPointFound) {
          decimalPointFound = true;
          continue;
        }

        throw InvalidArgTypeException("Decimal value '{}' has an invalid representation.", value_);
      }

      if (!digitFound) {
        throw InvalidArgTypeException("Decimal value '{}' has an invalid representation.", value_);
      }
    }

    explicit Decimal(std::string_view value)
      : Decimal(std::string{value})
    {}

    explicit Decimal(const char* value)
      : Decimal(value == nullptr ? std::string{} : std::string{value})
    {}

    [[nodiscard]]
    std::string_view value() const noexcept
    {
      return value_;
    }

    [[nodiscard]]
    std::size_t precision() const noexcept
    {
      std::size_t result = 0;
      for (const char character : value_) {
        if (character >= '0' && character <= '9') {
          ++result;
        }
      }
      return result;
    }

    [[nodiscard]]
    std::size_t scale() const noexcept
    {
      const std::size_t point = value_.find('.');
      return point == std::string::npos ? 0 : value_.size() - point - 1;
    }

    friend bool operator==(const Decimal&, const Decimal&) = default;

  private:
    std::string value_{"0"};
  };

  class Binary
  {
  public:
    Binary() = default;

    explicit Binary(std::vector<std::byte> value) noexcept
      : value_(std::move(value))
    {}

    explicit Binary(std::span<const std::byte> value)
      : value_(value.begin(), value.end())
    {}

    Binary(std::initializer_list<std::byte> value)
      : value_(value)
    {}

    [[nodiscard]]
    std::span<const std::byte> value() const noexcept
    {
      return value_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
      return value_.empty();
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
      return value_.size();
    }

    friend bool operator==(const Binary&, const Binary&) = default;

  private:
    std::vector<std::byte> value_;
  };
} // namespace worm::core
