#include <core/parameter.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

int main()
{
    using worm::core::Parameter;

    static_assert(std::variant_size_v<Parameter> == 5);
    static_assert(std::is_same_v<std::variant_alternative_t<0, Parameter>, std::nullptr_t>);
    static_assert(std::is_same_v<std::variant_alternative_t<1, Parameter>, std::int64_t>);

    const Parameter nullValue = nullptr;
    const Parameter integerValue = std::int64_t{42};
    const Parameter textValue = std::string{"Doctrine"};

    if (!std::holds_alternative<std::nullptr_t>(nullValue)
        || std::get<std::int64_t>(integerValue) != 42
        || std::get<std::string>(textValue) != "Doctrine") {
        std::cerr << "Parameter did not preserve a supported value.\n";
        return 1;
    }

    return 0;
}
