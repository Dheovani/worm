#pragma once

#include <core/model/entity.hpp>
#include <core/query/clauses.hpp>
#include <core/query/expression.hpp>
#include <errors/invalid-arg-exception.hpp>

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace worm::core
{

  enum class RelationshipKind
  {
    OneToOne,
    OneToMany,
    ManyToMany
  };

  enum class RelationshipLoadStrategy
  {
    Explicit,
    Eager,
    Lazy
  };

  namespace detail
  {

    constexpr void validateRelationshipText(std::string_view value, const char* message)
    {
      if (value.empty()) {
        throw InvalidArgException(message);
      }
    }

    inline Expression joinExpression(std::string_view leftAlias,
      std::string_view leftColumn,
      std::string_view rightAlias,
      std::string_view rightColumn)
    {
      return {
        std::string{leftAlias} + "." + std::string{leftColumn} + " = " + std::string{rightAlias} + "." +
          std::string{rightColumn},
        {},
      };
    }

  } // namespace detail

  template <Entity Owner, Entity Target>
  class DirectRelationship final
  {
  public:
    constexpr DirectRelationship(std::string_view name,
      RelationshipKind kind,
      std::string_view ownerColumn,
      std::string_view targetColumn,
      Join joinType = Join::Left,
      RelationshipLoadStrategy loadStrategy = RelationshipLoadStrategy::Explicit)
      : name_(name),
        kind_(kind),
        ownerColumn_(ownerColumn),
        targetColumn_(targetColumn),
        joinType_(joinType),
        loadStrategy_(loadStrategy)
    {
      if (kind != RelationshipKind::OneToOne && kind != RelationshipKind::OneToMany) {
        throw InvalidArgException("Direct relationships must be one-to-one or one-to-many.");
      }

      detail::validateRelationshipText(name_, "Relationship name must not be empty.");
      detail::validateRelationshipText(ownerColumn_, "Relationship owner column must not be empty.");
      detail::validateRelationshipText(targetColumn_, "Relationship target column must not be empty.");
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr RelationshipKind kind() const noexcept
    {
      return kind_;
    }

    [[nodiscard]]
    constexpr Table ownerTable() const noexcept
    {
      return std::remove_cvref_t<Owner>::table();
    }

    [[nodiscard]]
    constexpr Table targetTable() const noexcept
    {
      return std::remove_cvref_t<Target>::table();
    }

    [[nodiscard]]
    constexpr std::string_view ownerColumn() const noexcept
    {
      return ownerColumn_;
    }

    [[nodiscard]]
    constexpr std::string_view targetColumn() const noexcept
    {
      return targetColumn_;
    }

    [[nodiscard]]
    constexpr Join joinType() const noexcept
    {
      return joinType_;
    }

    [[nodiscard]]
    constexpr RelationshipLoadStrategy loadStrategy() const noexcept
    {
      return loadStrategy_;
    }

    [[nodiscard]]
    Relation relation(std::string_view ownerAlias, std::string_view targetAlias) const
    {
      detail::validateRelationshipText(ownerAlias, "Relationship owner alias must not be empty.");
      detail::validateRelationshipText(targetAlias, "Relationship target alias must not be empty.");

      const Source ownerSource{ownerTable().name(), ownerAlias};
      const Source targetSource{targetTable().name(), targetAlias};
      return Relation{joinType_,
        ownerSource,
        targetSource,
        detail::joinExpression(ownerAlias, ownerColumn_, targetAlias, targetColumn_)};
    }

  private:
    std::string_view name_;
    RelationshipKind kind_;
    std::string_view ownerColumn_;
    std::string_view targetColumn_;
    Join joinType_;
    RelationshipLoadStrategy loadStrategy_;
  };

  template <Entity Owner, Entity Target>
  class ManyToManyRelationship final
  {
  public:
    constexpr ManyToManyRelationship(std::string_view name,
      std::string_view joinTable,
      std::string_view ownerColumn,
      std::string_view joinOwnerColumn,
      std::string_view joinTargetColumn,
      std::string_view targetColumn,
      Join joinType = Join::Left,
      RelationshipLoadStrategy loadStrategy = RelationshipLoadStrategy::Explicit)
      : name_(name),
        joinTable_(joinTable),
        ownerColumn_(ownerColumn),
        joinOwnerColumn_(joinOwnerColumn),
        joinTargetColumn_(joinTargetColumn),
        targetColumn_(targetColumn),
        joinType_(joinType),
        loadStrategy_(loadStrategy)
    {
      detail::validateRelationshipText(name_, "Relationship name must not be empty.");
      detail::validateRelationshipText(joinTable_, "Relationship join table must not be empty.");
      detail::validateRelationshipText(ownerColumn_, "Relationship owner column must not be empty.");
      detail::validateRelationshipText(joinOwnerColumn_, "Relationship join owner column must not be empty.");
      detail::validateRelationshipText(joinTargetColumn_, "Relationship join target column must not be empty.");
      detail::validateRelationshipText(targetColumn_, "Relationship target column must not be empty.");
    }

    [[nodiscard]]
    constexpr std::string_view name() const noexcept
    {
      return name_;
    }

    [[nodiscard]]
    constexpr RelationshipKind kind() const noexcept
    {
      return RelationshipKind::ManyToMany;
    }

    [[nodiscard]]
    constexpr Table ownerTable() const noexcept
    {
      return std::remove_cvref_t<Owner>::table();
    }

    [[nodiscard]]
    constexpr Table targetTable() const noexcept
    {
      return std::remove_cvref_t<Target>::table();
    }

    [[nodiscard]]
    constexpr std::string_view joinTable() const noexcept
    {
      return joinTable_;
    }

    [[nodiscard]]
    constexpr std::string_view ownerColumn() const noexcept
    {
      return ownerColumn_;
    }

    [[nodiscard]]
    constexpr std::string_view joinOwnerColumn() const noexcept
    {
      return joinOwnerColumn_;
    }

    [[nodiscard]]
    constexpr std::string_view joinTargetColumn() const noexcept
    {
      return joinTargetColumn_;
    }

    [[nodiscard]]
    constexpr std::string_view targetColumn() const noexcept
    {
      return targetColumn_;
    }

    [[nodiscard]]
    constexpr Join joinType() const noexcept
    {
      return joinType_;
    }

    [[nodiscard]]
    constexpr RelationshipLoadStrategy loadStrategy() const noexcept
    {
      return loadStrategy_;
    }

    [[nodiscard]]
    std::vector<Relation> relations(
      std::string_view ownerAlias, std::string_view joinAlias, std::string_view targetAlias) const
    {
      detail::validateRelationshipText(ownerAlias, "Relationship owner alias must not be empty.");
      detail::validateRelationshipText(joinAlias, "Relationship join alias must not be empty.");
      detail::validateRelationshipText(targetAlias, "Relationship target alias must not be empty.");

      const Source ownerSource{ownerTable().name(), ownerAlias};
      const Source joinSource{joinTable_, joinAlias};
      const Source targetSource{targetTable().name(), targetAlias};

      return {
        Relation{joinType_,
          ownerSource,
          joinSource,
          detail::joinExpression(ownerAlias, ownerColumn_, joinAlias, joinOwnerColumn_)},
        Relation{joinType_,
          joinSource,
          targetSource,
          detail::joinExpression(joinAlias, joinTargetColumn_, targetAlias, targetColumn_)},
      };
    }

  private:
    std::string_view name_;
    std::string_view joinTable_;
    std::string_view ownerColumn_;
    std::string_view joinOwnerColumn_;
    std::string_view joinTargetColumn_;
    std::string_view targetColumn_;
    Join joinType_;
    RelationshipLoadStrategy loadStrategy_;
  };

  template <Entity Owner, Entity Target>
  [[nodiscard]]
  constexpr DirectRelationship<Owner, Target> oneToOne(std::string_view name,
    std::string_view ownerColumn,
    std::string_view targetColumn,
    Join joinType = Join::Left,
    RelationshipLoadStrategy loadStrategy = RelationshipLoadStrategy::Explicit)
  {
    return {name, RelationshipKind::OneToOne, ownerColumn, targetColumn, joinType, loadStrategy};
  }

  template <Entity Owner, Entity Target>
  [[nodiscard]]
  constexpr DirectRelationship<Owner, Target> oneToMany(std::string_view name,
    std::string_view ownerColumn,
    std::string_view targetColumn,
    Join joinType = Join::Left,
    RelationshipLoadStrategy loadStrategy = RelationshipLoadStrategy::Explicit)
  {
    return {name, RelationshipKind::OneToMany, ownerColumn, targetColumn, joinType, loadStrategy};
  }

  template <Entity Owner, Entity Target>
  [[nodiscard]]
  constexpr ManyToManyRelationship<Owner, Target> manyToMany(std::string_view name,
    std::string_view joinTable,
    std::string_view ownerColumn,
    std::string_view joinOwnerColumn,
    std::string_view joinTargetColumn,
    std::string_view targetColumn,
    Join joinType = Join::Left,
    RelationshipLoadStrategy loadStrategy = RelationshipLoadStrategy::Explicit)
  {
    return {name, joinTable, ownerColumn, joinOwnerColumn, joinTargetColumn, targetColumn, joinType, loadStrategy};
  }

  namespace detail
  {

    template <typename T>
    concept HasRelationships = requires { std::remove_cvref_t<T>::relationships(); };

  } // namespace detail

  template <Entity T>
  [[nodiscard]]
  constexpr auto relationships_of()
  {
    if constexpr (detail::HasRelationships<T>) {
      return std::remove_cvref_t<T>::relationships();
    } else {
      return std::tuple{};
    }
  }

  template <Entity T>
  inline constexpr std::size_t relationship_count =
    std::tuple_size_v<std::remove_cvref_t<decltype(relationships_of<T>())>>;

} // namespace worm::core
