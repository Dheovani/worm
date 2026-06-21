#include <reflection/field.hpp>
#include <reflection/metadata.hpp>

#include <iostream>
#include <string_view>

namespace
{
  struct Entity
  {
    int id = 0;
    std::string_view transientValue{};
  };

  constexpr auto idField = worm::reflection::field("id", &Entity::id,
                                                   worm::reflection::FieldMetadata{
                                                       .columnName = "entity_id",
                                                       .primaryKey = true,
                                                       .generated = true,
                                                   });

  constexpr auto ignoredField = worm::reflection::field("transientValue", &Entity::transientValue,
                                                        worm::reflection::FieldMetadata{
                                                            .ignored = true,
                                                        });

  constexpr auto defaultField = worm::reflection::field("id", &Entity::id);

  static_assert(idField.name() == "id");
  static_assert(idField.columnName() == "entity_id");
  static_assert(idField.isPrimaryKey());
  static_assert(idField.isGenerated());
  static_assert(!idField.isIgnored());
  static_assert(idField.isPersistent());

  static_assert(ignoredField.isIgnored());
  static_assert(!ignoredField.isPersistent());
  static_assert(defaultField.columnName() == defaultField.name());
  static_assert(!defaultField.isPrimaryKey());
  static_assert(!defaultField.isGenerated());
} // namespace

int main()
{
  if (idField.metadata().columnName != "entity_id" || !idField.metadata().primaryKey ||
      !idField.metadata().generated || idField.metadata().ignored) {
    std::cerr << "FieldDescriptor did not preserve its ORM metadata.\n";
    return 1;
  }

  return 0;
}
