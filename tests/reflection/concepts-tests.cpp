#include <reflection/concepts.hpp>

#include <tuple>

namespace
{
  struct ReflectableEntity
  {
    int id = 0;

    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &ReflectableEntity::id)};
    }
  };

  struct EmptyEntity
  {
    static constexpr auto reflect()
    {
      return std::tuple{};
    }
  };

  struct MissingReflection
  {};

  struct InvalidReflection
  {
    static constexpr int reflect()
    {
      return 42;
    }
  };

  struct ForeignOwner
  {
    int value = 0;
  };

  struct WrongOwner
  {
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("value", &ForeignOwner::value)};
    }
  };
} // namespace

int main()
{
  static_assert(worm::reflection::Reflectable<ReflectableEntity>);
  static_assert(worm::reflection::Reflectable<const ReflectableEntity&>);
  static_assert(worm::reflection::Reflectable<EmptyEntity>);
  static_assert(!worm::reflection::Reflectable<MissingReflection>);
  static_assert(!worm::reflection::Reflectable<InvalidReflection>);
  static_assert(!worm::reflection::Reflectable<WrongOwner>);

  return 0;
}
