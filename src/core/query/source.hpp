#pragma once

#include <core/query/expression.hpp>

#include <optional>
#include <string_view>

namespace worm::core
{

  struct Source
  {
    const std::string_view name;
    const std::optional<std::string_view> alias;

    Source(std::string_view name, std::optional<std::string_view> alias = std::nullopt) noexcept
      : name(name),
        alias(alias)
    {}
  };

  struct Field
  {
    const std::string_view name;
    const Source source;

    Field(std::string_view name, Source source) noexcept
      : name(name),
        source(source)
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
    const Join joinType;
    const Source baseSource;
    const Source joinedSource;
    const Expression condition;

    Relation(Join joinType, Source baseSource, Source joinedSource, Expression condition) noexcept
      : joinType(joinType),
        baseSource(baseSource),
        joinedSource(joinedSource),
        condition(condition)
    {}
  };

} // namespace worm::core
