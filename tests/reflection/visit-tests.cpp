#include <reflection/visit.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace
{
	struct Entity
	{
		int id = 5;
		std::string name = "Worm";

		static constexpr auto reflect()
		{
			return std::tuple{
				worm::reflection::Field("id", &Entity::id),
				worm::reflection::Field("name", &Entity::name)
			};
		}
	};
}

int main()
{
	static_assert(worm::reflection::FieldCount<Entity> == 2);

	Entity entity;
	std::vector<std::string> names;

	worm::reflection::ForEachField(entity, [&names](const auto& field, auto& value) {
		names.emplace_back(field.name());

		using Value = std::remove_cvref_t<decltype(value)>;
		if constexpr (std::is_same_v<Value, int>)
			value = 10;
		else if constexpr (std::is_same_v<Value, std::string>)
			value = "Doctrine";
	});

	if (names != std::vector<std::string>{"id", "name"}
		|| entity.id != 10 || entity.name != "Doctrine") {
		std::cerr << "Mutable field visitation produced an unexpected result.\n";
		return 1;
	}

	const Entity& constEntity = entity;
	std::size_t visited = 0;
	worm::reflection::ForEachField(constEntity, [&visited](const auto&, const auto&) {
		++visited;
	});

	if (visited != worm::reflection::FieldCount<Entity>) {
		std::cerr << "Const field visitation skipped fields.\n";
		return 1;
	}

	return 0;
}
