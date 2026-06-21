#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <reflection/concepts.hpp>

namespace worm::reflection
{
	template <Reflectable T>
	inline constexpr std::size_t FieldCount = std::tuple_size_v<
		std::remove_cvref_t<decltype(std::remove_cvref_t<T>::reflect())>
	>;

	template <Reflectable T, typename Visitor>
	constexpr void ForEachField(T&& object, Visitor&& visitor)
	{
		const auto fields = std::remove_cvref_t<T>::reflect();

		std::apply(
			[&object, &visitor](const auto&... field) {
				(
					std::invoke(visitor, field, field.get(object)),
					...
				);
			},
			fields
		);
	}
}
