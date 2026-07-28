#pragma once

#include <errors/invalid-arg-type-exception.hpp>
#include <utils/helpers.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
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
    using remove_optional_t = typename std::remove_cvref_t<T>::value_type;

    template <typename T>
    inline constexpr bool is_valid_parameter_type =
      std::is_enum_v<std::remove_cvref_t<T>> ||
      std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
      std::same_as<std::remove_cvref_t<T>, bool> ||
      std::floating_point<std::remove_cvref_t<T>> ||
      (std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>);

    template <typename T>
    consteval bool is_encodable_parameter()
    {
      if constexpr (is_valid_parameter_type<T> || utils::is_string_like<T>) {
        return true;
      } else if constexpr (utils::is_optional_v<T>) {
        return is_encodable_parameter<remove_optional_t<T>>();
      } else {
        return false;
      }
    }

    template <typename T>
    consteval bool is_decodable_parameter()
    {
      using Value = std::remove_cvref_t<T>;

      if constexpr (is_valid_parameter_type<T> || std::same_as<Value, std::string> || std::same_as<Value, std::string_view>) {
        return true;
      } else if constexpr (utils::is_optional_v<T>) {
        return is_decodable_parameter<remove_optional_t<T>>();
      } else {
        return false;
      }
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
