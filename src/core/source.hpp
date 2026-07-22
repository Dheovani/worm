#pragma once

#include <core/expression.hpp>

#include <optional>
#include <string_view>

namespace worm::core
{

  struct Source
  {
    const std::string_view name_;
    const std::optional<std::string_view> alias_;

		Source(std::string_view name, std::optional<std::string_view> alias = std::nullopt) noexcept
      : name_(name), alias_(alias)
    {}
	};

	struct Field
	{
    const std::string_view name_;
    const Source source_;

		Field(std::string_view name, Source source) noexcept
			: name_(name), source_(source)
		{}
	};

  enum class Join
  {
    Inner,
    Left,
    Right,
    Full
  };

	struct Relation
	{
    const Join type_;
    const Source left_;
    const Source right_;
    const Expression expression_;

		Relation(Join type, Source left, Source right, Expression expression) noexcept
			: type_(type), left_(left), right_(right), expression_(expression)
		{}
	};

} // namespace worm::core
