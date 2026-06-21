#include <reflection/snapshot.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{
  struct Entity
  {
    int id = 7;
    std::string name = "Worm";
    int transientValue = 10;

    static constexpr auto reflect()
    {
      return std::tuple{
          worm::reflection::field("id", &Entity::id),
          worm::reflection::field("name", &Entity::name),
          worm::reflection::field("transientValue", &Entity::transientValue,
                                  worm::reflection::FieldMetadata{
                                      .ignored = true,
                                  }),
      };
    }
  };

  struct NonComparable
  {
    int value = 0;
  };

  struct InvalidSnapshotEntity
  {
    NonComparable value;

    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("value", &InvalidSnapshotEntity::value)};
    }
  };

  static_assert(worm::reflection::Snapshotable<Entity>);
  static_assert(!worm::reflection::Snapshotable<InvalidSnapshotEntity>);
  static_assert(std::tuple_size_v<worm::reflection::EntitySnapshot<Entity>> == 3);
} // namespace

int main()
{
  Entity entity;
  const auto snapshot = worm::reflection::make_snapshot(entity);

  if (worm::reflection::is_dirty(entity, snapshot) ||
      worm::reflection::changed_field_count(entity, snapshot) != 0) {
    std::cerr << "A new snapshot was reported as dirty.\n";
    return 1;
  }

  entity.transientValue = 99;
  if (worm::reflection::is_dirty(entity, snapshot)) {
    std::cerr << "An ignored field changed the persistent dirty state.\n";
    return 1;
  }

  entity.id = 8;
  entity.name = "Doctrine";
  std::vector<std::string> changedNames;
  bool idValuesAreCorrect = false;
  bool nameValuesAreCorrect = false;

  const std::size_t changed = worm::reflection::for_each_changed_field(
      entity, snapshot,
      [&](const auto& descriptor, const auto& previousValue, const auto& currentValue) {
        changedNames.emplace_back(descriptor.name());

        using Value = std::remove_cvref_t<decltype(currentValue)>;
        if constexpr (std::is_same_v<Value, int>) {
          if (descriptor.name() == "id")
            idValuesAreCorrect = previousValue == 7 && currentValue == 8;
        } else if constexpr (std::is_same_v<Value, std::string>) {
          nameValuesAreCorrect = previousValue == "Worm" && currentValue == "Doctrine";
        }
      });

  if (changed != 2 || changedNames != std::vector<std::string>{"id", "name"} ||
      !idValuesAreCorrect || !nameValuesAreCorrect ||
      worm::reflection::changed_field_count(entity, snapshot) != 2 ||
      !worm::reflection::is_dirty(entity, snapshot)) {
    std::cerr << "Snapshot change detection returned an unexpected result.\n";
    return 1;
  }

  return 0;
}
