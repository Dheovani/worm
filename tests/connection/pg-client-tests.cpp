#include <connection/pg-client.hpp>

#include <type_traits>
#include <utility>

int main()
{
  using Client = worm::connection::PgClient;

  static_assert(std::is_base_of_v<worm::connection::Client, Client>);
  static_assert(std::is_final_v<Client>);
  static_assert(!std::is_copy_constructible_v<Client>);
  static_assert(!std::is_copy_assignable_v<Client>);
  static_assert(
      std::is_same_v<decltype(Client::getInstance(std::declval<const Json::Value&>())), Client&>);

  return 0;
}
