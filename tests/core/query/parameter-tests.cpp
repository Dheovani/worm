#include <core/query/expression.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace
{
  enum class Status : std::int64_t
  {
    Inactive = 0,
    Active = 1
  };
} // namespace

int main()
{
  using worm::core::Parameter;

  static_assert(std::variant_size_v<Parameter> == 5);
  static_assert(std::is_same_v<std::variant_alternative_t<0, Parameter>, std::nullptr_t>);
  static_assert(std::is_same_v<std::variant_alternative_t<1, Parameter>, std::int64_t>);
  static_assert(worm::core::EncodableParameter<int>);
  static_assert(worm::core::EncodableParameter<std::optional<std::string>>);
  static_assert(worm::core::EncodableParameter<Status>);
  static_assert(worm::core::DecodableParameter<int>);
  static_assert(worm::core::DecodableParameter<std::optional<std::string>>);
  static_assert(worm::core::DecodableParameter<Status>);

  const Parameter nullValue = nullptr;
  const Parameter integerValue = std::int64_t{42};
  const Parameter textValue = std::string{"Doctrine"};

  if (!std::holds_alternative<std::nullptr_t>(nullValue) || std::get<std::int64_t>(integerValue) != 42 ||
      std::get<std::string>(textValue) != "Doctrine") {
    std::cerr << "Parameter did not preserve a supported value.\n";
    return 1;
  }

  const Parameter encodedNull = worm::core::encode(nullptr);
  const Parameter encodedEmptyText = worm::core::encode(std::string{""});
  const Parameter encodedZero = worm::core::encode(0);
  const Parameter encodedFalse = worm::core::encode(false);
  const Parameter encodedOptionalNull = worm::core::encode(std::optional<std::string>{});
  const Parameter encodedOptionalText = worm::core::encode(std::optional<std::string>{"Ada"});
  const Parameter encodedEnum = worm::core::encode(Status::Active);

  if (!std::holds_alternative<std::nullptr_t>(encodedNull) ||
      !std::holds_alternative<std::string>(encodedEmptyText) ||
      !std::get<std::string>(encodedEmptyText).empty() ||
      std::get<std::int64_t>(encodedZero) != 0 ||
      std::get<bool>(encodedFalse) != false ||
      !std::holds_alternative<std::nullptr_t>(encodedOptionalNull) ||
      std::get<std::string>(encodedOptionalText) != "Ada" ||
      std::get<std::int64_t>(encodedEnum) != 1) {
    std::cerr << "Parameter encoding did not preserve distinct SQL values.\n";
    return 1;
  }

  const auto decodedInteger = worm::core::decode<int>(Parameter{std::int64_t{7}});
  const auto decodedNull = worm::core::decode<std::nullptr_t>(Parameter{nullptr});
  const auto decodedOptionalNull = worm::core::decode<std::optional<std::string>>(Parameter{nullptr});
  const auto decodedOptionalText = worm::core::decode<std::optional<std::string>>(Parameter{std::string{"Grace"}});
  const auto decodedEnum = worm::core::decode<Status>(Parameter{std::int64_t{1}});
  const auto invalidNull = worm::core::decode<int>(Parameter{nullptr});
  const auto invalidType = worm::core::decode<int>(Parameter{std::string{"not an integer"}});
  const auto outOfRange = worm::core::decode<std::int8_t>(Parameter{std::int64_t{128}});

  if (std::get<int>(decodedInteger) != 7 ||
      !std::holds_alternative<std::nullptr_t>(decodedNull) ||
      std::get<std::optional<std::string>>(decodedOptionalNull).has_value() ||
      std::get<std::optional<std::string>>(decodedOptionalText).value() != "Grace" ||
      std::get<Status>(decodedEnum) != Status::Active ||
      std::get<worm::core::DecodeError>(invalidNull) != worm::core::DecodeError::NullValue ||
      std::get<worm::core::DecodeError>(invalidType) != worm::core::DecodeError::IncompatibleType ||
      std::get<worm::core::DecodeError>(outOfRange) != worm::core::DecodeError::OutOfRange) {
    std::cerr << "Parameter decoding did not preserve type conversion semantics.\n";
    return 1;
  }

  return 0;
}
