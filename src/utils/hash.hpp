#pragma once

#include <cstdint>

namespace worm
{
	typedef unsigned long long hash;

	// Creates a HashCode from a given string.
	// The generated hash will always be equal according to the given string
	constexpr hash HashCode(const char* key) noexcept(true)
	{
		hash value = 0;
		size_t len = 0;
		while (key[len] != '\0')
			++len;

		for (uint64_t i = 0; i < len; ++i) {
			value = value * 37 + key[i];
		}

		return value;
	}
}
