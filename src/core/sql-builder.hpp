#pragma once

#include <concepts>
#include <vector>

#include <core/source.hpp>

namespace worm::core
{

	class Builder
  {
	public:
    virtual const std::string_view select(
			const std::vector<worm::core::Field>& fields,
			const Source& source,
      const std::vector<Relation>& relations) const noexcept = 0;
	};

	class PgBuilder : public Builder
  {
	public:
    const std::string_view select(
			const std::vector<worm::core::Field>& fields,
			const Source& source,
			const std::vector<Relation>& relations) const noexcept override;
	};

	template <typename T>
  concept SqlBuilder = std::derived_from<std::remove_cvref_t<T>, Builder>;

	[[nodiscard]]
  const auto getBuilder();

}
