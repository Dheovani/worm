#include <core/model/relationship.hpp>

#include <errors/invalid-arg-exception.hpp>
#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>

namespace
{
  struct User
  {
    std::int64_t id{};

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    [[nodiscard]]
    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &User::id)};
    }
  };

  struct Profile
  {
    std::int64_t id{};
    std::int64_t userId{};

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"profiles"};
    }

    [[nodiscard]]
    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_profiles", {worm::core::Column{"id", table()}}};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &Profile::id),
        worm::reflection::field("userId", &Profile::userId, {.columnName = "user_id"})};
    }
  };

  struct Post
  {
    std::int64_t id{};
    std::int64_t userId{};

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"posts"};
    }

    [[nodiscard]]
    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_posts", {worm::core::Column{"id", table()}}};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &Post::id),
        worm::reflection::field("userId", &Post::userId, {.columnName = "user_id"})};
    }
  };

  struct Role
  {
    std::int64_t id{};

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"roles"};
    }

    [[nodiscard]]
    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_roles", {worm::core::Column{"id", table()}}};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &Role::id)};
    }
  };

  struct UserWithRelationships
  {
    std::int64_t id{};

    [[nodiscard]]
    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    [[nodiscard]]
    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_users", {worm::core::Column{"id", table()}}};
    }

    [[nodiscard]]
    static constexpr auto reflect()
    {
      return std::tuple{worm::reflection::field("id", &UserWithRelationships::id)};
    }

    [[nodiscard]]
    static constexpr auto relationships()
    {
      return std::tuple{worm::core::oneToOne<UserWithRelationships, Profile>("profile", "id", "user_id"),
        worm::core::oneToMany<UserWithRelationships, Post>("posts", "id", "user_id"),
        worm::core::manyToMany<UserWithRelationships, Role>("roles", "user_roles", "id", "user_id", "role_id", "id")};
    }
  };
} // namespace

int main()
{
  using worm::core::Join;
  using worm::core::RelationshipKind;
  using worm::core::RelationshipLoadStrategy;

  constexpr worm::core::CascadePolicy defaultCascade;
  static_assert(defaultCascade.empty());
  static_assert(!defaultCascade.persist);
  static_assert(!defaultCascade.update);
  static_assert(!defaultCascade.remove);
  static_assert(!defaultCascade.refresh);

  static_assert(worm::core::relationship_count<User> == 0);
  static_assert(worm::core::relationship_count<UserWithRelationships> == 3);

  constexpr auto profile = worm::core::oneToOne<User, Profile>("profile", "id", "user_id");
  static_assert(profile.kind() == RelationshipKind::OneToOne);
  static_assert(profile.name() == "profile");
  static_assert(profile.ownerColumn() == "id");
  static_assert(profile.targetColumn() == "user_id");
  static_assert(profile.loadStrategy() == RelationshipLoadStrategy::Explicit);
  static_assert(profile.cascadePolicy().empty());
  static_assert(!profile.orphanRemoval());

  const auto profileRelation = profile.relation("u", "p");
  if (profileRelation.joinType != Join::Left || profileRelation.baseSource.name != "users" ||
      profileRelation.joinedSource.name != "profiles" || profileRelation.condition.sql != "u.id = p.user_id" ||
      !profileRelation.condition.parameters.empty()) {
    std::cerr << "One-to-one relationship did not produce the expected join relation.\n";
    return 1;
  }

  constexpr worm::core::CascadePolicy postCascade{.persist = true, .update = true, .remove = true};
  static_assert(!postCascade.empty());

  constexpr auto posts = worm::core::oneToMany<User, Post>(
    "posts",
    "id",
    "user_id",
    Join::Inner,
    RelationshipLoadStrategy::Eager,
    postCascade,
    true);
  static_assert(posts.kind() == RelationshipKind::OneToMany);
  static_assert(posts.loadStrategy() == RelationshipLoadStrategy::Eager);
  static_assert(posts.cascadePolicy().persist);
  static_assert(posts.cascadePolicy().update);
  static_assert(posts.cascadePolicy().remove);
  static_assert(!posts.cascadePolicy().refresh);
  static_assert(posts.orphanRemoval());
  const auto postsRelation = posts.relation("u", "po");
  if (postsRelation.joinType != Join::Inner || postsRelation.condition.sql != "u.id = po.user_id") {
    std::cerr << "One-to-many relationship did not preserve its join type or condition.\n";
    return 1;
  }

  constexpr auto roles = worm::core::manyToMany<User, Role>(
    "roles",
    "user_roles",
    "id",
    "user_id",
    "role_id",
    "id",
    Join::Left,
    RelationshipLoadStrategy::Lazy);
  static_assert(roles.kind() == RelationshipKind::ManyToMany);
  static_assert(roles.joinTable() == "user_roles");
  static_assert(roles.loadStrategy() == RelationshipLoadStrategy::Lazy);
  static_assert(roles.cascadePolicy().empty());
  static_assert(!roles.orphanRemoval());

  const auto roleRelations = roles.relations("u", "ur", "r");
  if (roleRelations.size() != 2 || roleRelations[0].joinedSource.name != "user_roles" ||
      roleRelations[0].condition.sql != "u.id = ur.user_id" || roleRelations[1].joinedSource.name != "roles" ||
      roleRelations[1].condition.sql != "ur.role_id = r.id") {
    std::cerr << "Many-to-many relationship did not produce the expected join chain.\n";
    return 1;
  }

  try {
    static_cast<void>(worm::core::oneToOne<User, Profile>("", "id", "user_id"));
    std::cerr << "Relationship accepted an empty name.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  try {
    static_cast<void>(profile.relation("", "p"));
    std::cerr << "Relationship accepted an empty alias.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  return 0;
}
