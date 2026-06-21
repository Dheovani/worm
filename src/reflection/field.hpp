#pragma once

#include <string_view>
#include <type_traits>

#include <reflection/metadata.hpp>

namespace worm
{
  namespace reflection
  {
    template <typename Owner, typename Value> class FieldDescriptor
    {
    public:
      using owner_type = Owner;
      using value_type = Value;
      using member_pointer = Value Owner::*;

      constexpr FieldDescriptor(std::string_view name, member_pointer member,
                                FieldMetadata metadata = {}) noexcept
          : name_(name), member_(member), metadata_(metadata)
      {}

      [[nodiscard]] constexpr std::string_view name() const noexcept
      {
        return name_;
      }

      [[nodiscard]] constexpr member_pointer member() const noexcept
      {
        return member_;
      }

      [[nodiscard]] constexpr std::string_view columnName() const noexcept
      {
        return metadata_.columnName.empty() ? name_ : metadata_.columnName;
      }

      [[nodiscard]] constexpr bool isPrimaryKey() const noexcept
      {
        return metadata_.primaryKey;
      }

      [[nodiscard]] constexpr bool isGenerated() const noexcept
      {
        return metadata_.generated;
      }

      [[nodiscard]] constexpr bool isIgnored() const noexcept
      {
        return metadata_.ignored;
      }

      [[nodiscard]] constexpr bool isPersistent() const noexcept
      {
        return !metadata_.ignored;
      }

      [[nodiscard]] constexpr const FieldMetadata& metadata() const noexcept
      {
        return metadata_;
      }

      [[nodiscard]] constexpr Value& get(Owner& object) const noexcept
      {
        return object.*member_;
      }

      [[nodiscard]] constexpr const Value& get(const Owner& object) const noexcept
      {
        return object.*member_;
      }

    private:
      std::string_view name_;
      member_pointer member_;
      FieldMetadata metadata_;
    };

    template <typename Owner, typename Value>
    FieldDescriptor(std::string_view, Value Owner::*) -> FieldDescriptor<Owner, Value>;

    template <typename Owner, typename Value>
    [[nodiscard]] constexpr auto field(std::string_view name, Value Owner::* member,
                                       FieldMetadata metadata = {}) noexcept
    {
      return FieldDescriptor<Owner, Value>{name, member, metadata};
    }

    template <typename T> struct is_field_descriptor : std::false_type
    {};

    template <typename Owner, typename Value>
    struct is_field_descriptor<FieldDescriptor<Owner, Value>> : std::true_type
    {};

    template <typename T>
    inline constexpr bool is_field_descriptor_v =
        is_field_descriptor<std::remove_cvref_t<T>>::value;
  } // namespace reflection
} // namespace worm
