#include <reflection/field.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
  class Entity
  {
  public:
    Entity(int id, std::string name) : id_(id), name_(std::move(name)) {}

    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &Entity::id_),
                        worm::reflection::field("name", &Entity::name_)};
    }

  private:
    int id_;
    std::string name_;
  };
} // namespace

int main()
{
  Entity entity{7, "Worm"};
  const auto [id, name] = Entity::reflect();

  static_assert(worm::reflection::is_field_descriptor_v<decltype(id)>);
  static_assert(std::is_same_v<decltype(id)::owner_type, Entity>);
  static_assert(std::is_same_v<decltype(id)::value_type, int>);

  if (id.name() != "id" || name.name() != "name" || id.get(entity) != 7 ||
      name.get(entity) != "Worm") {
    std::cerr << "Field descriptors returned unexpected metadata or values.\n";
    return 1;
  }

  id.get(entity) = 9;
  name.get(entity) = "Doctrine";
  const Entity& constEntity = entity;

  if (id.get(constEntity) != 9 || name.get(constEntity) != "Doctrine") {
    std::cerr << "Field descriptors did not preserve const or mutable access.\n";
    return 1;
  }

  return 0;
}
