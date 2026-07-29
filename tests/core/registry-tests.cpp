#include <core/registry.hpp>

#include <reflection/field.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
  struct User
  {
    std::int64_t id{};
    std::string name;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"users"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &User::id, {.primaryKey = true}),
        worm::reflection::field("name", &User::name)};
    }
  };

  struct Post
  {
    std::int64_t id{};
    std::string title;

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"posts"};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{
        worm::reflection::field("id", &Post::id, {.primaryKey = true}),
        worm::reflection::field("title", &Post::title)};
    }
  };
} // namespace

int main()
{
  worm::core::InstanceRegistry<User> users;
  users.add(1, User{.id = 1, .name = "Ada"});

  if (!users.has(1) || !users.has(User{.id = 1, .name = "Ignored"}) || users.count() != 1) {
    std::cerr << "InstanceRegistry did not store an added entity by primary key.\n";
    return 1;
  }

  const std::shared_ptr<User> added = users.get(1);
  if (!added || added->name != "Ada" || !users.hasSnapshot(1) || users.isDirty(1) || users.changedFieldCount(1) != 0) {
    std::cerr << "InstanceRegistry did not return the added entity.\n";
    return 1;
  }

  added->name = "Byron";
  std::vector<std::string> changedFields;
  const std::size_t changedFieldsCount =
    users.forEachChangedField(1, [&](const auto& descriptor, const auto&, const auto&) {
      changedFields.emplace_back(descriptor.name());
    });

  if (!users.isDirty(1) ||
      users.changedFieldCount(1) != 1 ||
      changedFieldsCount != 1 ||
      changedFields != std::vector<std::string>{"name"}) {
    std::cerr << "InstanceRegistry did not detect changed fields against the stored snapshot.\n";
    return 1;
  }

  users.refreshSnapshot(1);
  if (users.isDirty(1) || users.changedFieldCount(1) != 0) {
    std::cerr << "InstanceRegistry did not refresh the stored snapshot.\n";
    return 1;
  }

  users.add(1, User{.id = 1, .name = "Grace"});
  if (users.get(1)->name != "Byron" || users.count() != 1 || users.isDirty(1)) {
    std::cerr << "InstanceRegistry add replaced an existing entity.\n";
    return 1;
  }

  users.put(1, User{.id = 1, .name = "Grace"});
  if (users.get(1)->name != "Grace" || users.count() != 1 || users.isDirty(1)) {
    std::cerr << "InstanceRegistry put did not replace an existing entity.\n";
    return 1;
  }

  users.emplace(1, User{.id = 1, .name = "Lovelace"});
  if (users.get(1)->name != "Grace" || users.count() != 1 || users.isDirty(1)) {
    std::cerr << "InstanceRegistry emplace replaced an existing entity.\n";
    return 1;
  }

  users.emplace(2, User{.id = 2, .name = "Lovelace"});
  if (!users.has(2) || !users.hasSnapshot(2) || users.get(2)->name != "Lovelace" || users.count() != 2) {
    std::cerr << "InstanceRegistry emplace did not insert a missing entity.\n";
    return 1;
  }

  users.remove(1);
  if (users.has(1) || users.hasSnapshot(1) || users.get(1) != nullptr || users.count() != 1) {
    std::cerr << "InstanceRegistry remove did not erase the entity.\n";
    return 1;
  }

  worm::core::Registry registry;
  auto& registryUsers = registry.instances<User>();
  auto& sameRegistryUsers = registry.instances<User>();
  auto& registryPosts = registry.instances<Post>();

  registryUsers.put(7, User{.id = 7, .name = "Ada"});
  registryPosts.put(7, Post{.id = 7, .title = "Post"});

  if (&registryUsers != &sameRegistryUsers ||
      !sameRegistryUsers.has(7) ||
      sameRegistryUsers.get(7)->name != "Ada" ||
      !registryPosts.has(7) ||
      registryPosts.get(7)->title != "Post") {
    std::cerr << "Registry did not preserve isolated typed instance registries.\n";
    return 1;
  }

  return 0;
}
