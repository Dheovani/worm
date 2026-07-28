#pragma once

#include <errors/invalid-arg-type-exception.hpp>
#include <utils/helpers.hpp>

#include <concepts>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace worm::core
{

  using Parameter = std::variant<
    std::nullptr_t,
    std::int64_t,
    double,
    bool,
    std::string>;

  namespace detail
  {
    template <typename T>
    inline constexpr bool is_valid_parameter_type =
      std::is_enum_v<std::remove_cvref_t<T>> ||
      std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
      std::same_as<std::remove_cvref_t<T>, bool> ||
      utils::is_date_type<T> ||
      std::floating_point<std::remove_cvref_t<T>> ||
      (std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>);

    template <typename T>
    consteval bool is_encodable_parameter()
    {
      if constexpr (is_valid_parameter_type<T> || utils::is_string_like<T>) {
        return true;
      } else if constexpr (utils::is_optional_v<T>) {
        return is_encodable_parameter<utils::remove_optional_t<T>>();
      } else {
        return false;
      }
    }

    template <typename T>
    consteval bool is_decodable_parameter()
    {
      using Value = std::remove_cvref_t<T>;

      if constexpr (
        is_valid_parameter_type<T> ||
        std::same_as<Value, std::string> ||
        std::same_as<Value, std::string_view>) {
        return true;
      } else if constexpr (utils::is_optional_v<T>) {
        return is_decodable_parameter<utils::remove_optional_t<T>>();
      } else {
        return false;
      }
    }

    [[nodiscard]]
    inline std::string format_date(std::chrono::sys_days value)
    {
      const std::chrono::year_month_day date{value};
      std::ostringstream output;
      output << std::setw(4) << std::setfill('0') << static_cast<int>(date.year()) << '-'
             << std::setw(2) << std::setfill('0') << static_cast<unsigned>(date.month()) << '-'
             << std::setw(2) << std::setfill('0') << static_cast<unsigned>(date.day());

      return output.str();
    }

    [[nodiscard]]
    inline std::optional<std::chrono::sys_days> parse_date(std::string_view value)
    {
      if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return std::nullopt;
      }

      const auto digit = [](char character) -> std::optional<int>
      {
        if (character < '0' || character > '9') {
          return std::nullopt;
        }

        return character - '0';
      };

      const auto year0 = digit(value[0]);
      const auto year1 = digit(value[1]);
      const auto year2 = digit(value[2]);
      const auto year3 = digit(value[3]);
      const auto month0 = digit(value[5]);
      const auto month1 = digit(value[6]);
      const auto day0 = digit(value[8]);
      const auto day1 = digit(value[9]);
      if (!year0 || !year1 || !year2 || !year3 || !month0 || !month1 || !day0 || !day1) {
        return std::nullopt;
      }

      const int year = (*year0 * 1000) + (*year1 * 100) + (*year2 * 10) + *year3;
      const unsigned month = static_cast<unsigned>((*month0 * 10) + *month1);
      const unsigned day = static_cast<unsigned>((*day0 * 10) + *day1);
      const std::chrono::year_month_day date{
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}};
      if (!date.ok()) {
        return std::nullopt;
      }

      return std::chrono::sys_days{date};
    }
  } // namespace detail

  enum class DecodeError
  {
    IncompatibleType,
    OutOfRange,
    NullValue
  };

  template <typename T>
  concept EncodableParameter = detail::is_encodable_parameter<T>();

  template <typename T>
  concept DecodableParameter = detail::is_decodable_parameter<T>();

  template <typename T>
  using DecodeResult = std::variant<std::remove_cvref_t<T>, DecodeError>;

  template <EncodableParameter T>
  Parameter encode(const T& value)
  {
    using Value = std::remove_cvref_t<T>;

    if constexpr (std::same_as<Value, std::nullptr_t>) {
      return nullptr;
    } else if constexpr (std::same_as<Value, bool>) {
      return value;
    } else if constexpr (std::integral<Value>) {
      if (!std::in_range<std::int64_t>(value))
        throw InvalidArgTypeException("Integral value cannot be represented as std::int64_t");

      return static_cast<std::int64_t>(value);
    } else if constexpr (std::floating_point<Value>) {
      return static_cast<double>(value);
    } else if constexpr (utils::is_date_type<Value>) {
      return detail::format_date(value);
    } else if constexpr (utils::is_string_like<T>) {
      return std::string{value};
    } else if constexpr (std::is_enum_v<Value>) {
      return encode(static_cast<std::underlying_type_t<Value>>(value));
    } else {
      if (!value.has_value()) {
        return nullptr;
      }

      return encode(value.value());
    }
  }

  template <DecodableParameter T>
  DecodeResult<T> decode(const Parameter& parameter)
  {
    using Value = std::remove_cvref_t<T>;

    if constexpr (utils::is_optional_v<Value>) {
      using Inner = typename Value::value_type;

      if (std::holds_alternative<std::nullptr_t>(parameter)) {
        return Value{std::nullopt};
      }

      const DecodeResult<Inner> decoded = decode<Inner>(parameter);
      if (std::holds_alternative<DecodeError>(decoded)) {
        return std::get<DecodeError>(decoded);
      }

      return Value{std::get<Inner>(decoded)};
    } else if constexpr (std::is_enum_v<Value>) {
      using Underlying = std::underlying_type_t<Value>;
      const DecodeResult<Underlying> decoded = decode<Underlying>(parameter);

      if (std::holds_alternative<DecodeError>(decoded)) {
        return std::get<DecodeError>(decoded);
      }

      return static_cast<Value>(std::get<Underlying>(decoded));
    } else {
      return std::visit(
        []<typename Stored>(const Stored& stored) -> DecodeResult<Value>
        {
          using Source = std::remove_cvref_t<Stored>;

          if constexpr (std::same_as<Value, std::nullptr_t>) {
            if constexpr (std::same_as<Source, std::nullptr_t>) {
              return nullptr;
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::same_as<Source, std::nullptr_t>) {
            return DecodeError::NullValue;
          } else if constexpr (std::same_as<Value, bool>) {
            if constexpr (std::same_as<Source, bool>) {
              return stored;
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::integral<Value>) {
            if constexpr (std::same_as<Source, std::int64_t>) {
              if (!std::in_range<Value>(stored)) {
                return DecodeError::OutOfRange;
              }

              return static_cast<Value>(stored);
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::floating_point<Value>) {
            if constexpr (std::same_as<Source, double>) {
              if (stored < std::numeric_limits<Value>::lowest() || stored > std::numeric_limits<Value>::max()) {
                return DecodeError::OutOfRange;
              }

              return static_cast<Value>(stored);
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::same_as<Value, std::string>) {
            if constexpr (std::same_as<Source, std::string>) {
              return stored;
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::same_as<Value, std::string_view>) {
            if constexpr (std::same_as<Source, std::string>) {
              return std::string_view{stored};
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (utils::is_date_type<Value>) {
            if constexpr (std::same_as<Source, std::string>) {
              const std::optional<std::chrono::sys_days> date = detail::parse_date(stored);
              if (!date) {
                return DecodeError::IncompatibleType;
              }

              return *date;
            } else {
              return DecodeError::IncompatibleType;
            }
          } else if constexpr (std::same_as<Source, std::nullptr_t>) {
            return nullptr;
          }
        },
        parameter);
    }
  }

  enum class Comparison
  {
    Equal,
    NotEqual,
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual,
    Like
  };

  struct Expression
  {
    std::string sql;
    std::vector<Parameter> parameters;
  };

} // namespace worm::core
