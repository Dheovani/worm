#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>
#include <vector>

#include <reflection/concepts.hpp>
#include <reflection/snapshot.hpp>

namespace worm::core
{

  struct Table
  {
  public:
    constexpr Table(std::string_view name) noexcept
      : name_(name)
    {}

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

  private:
    const std::string_view name_;
  };

  template <typename Derived>
  struct TableEntity : public Table
  {
    using EntityType = Derived;
  };

  template <typename T>
  concept Entity =
    std::derived_from<std::remove_cvref_t<T>, TableEntity<std::remove_cvref_t<T>>> &&
    reflection::Reflectable<std::remove_cvref_t<T>> &&
    reflection::Snapshotable<std::remove_cvref_t<T>>;

} // namespace worm::core
