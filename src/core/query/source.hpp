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

  enum class Aggregate
  {
    Count,
    Sum,
    Average,
    Minimum,
    Maximum
  };

  struct Field
  {
    const std::string_view name;
    const Source source;
    const std::optional<std::string_view> alias;
    const std::optional<Aggregate> aggregate;

    Field(std::string_view name, Source source, std::optional<std::string_view> alias = std::nullopt) noexcept
      : name(name),
        source(source),
        alias(alias),
        aggregate(std::nullopt)
    {}

    Field(std::string_view name,
      Source source,
      Aggregate aggregate,
      std::optional<std::string_view> alias = std::nullopt) noexcept
      : name(name),
        source(source),
        alias(alias),
        aggregate(aggregate)
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
