#include <connection/drivers/mysql-client.hpp>

#include <type_traits>

int main()
{
  using Client = worm::connection::MySqlClient;

  static_assert(std::is_base_of_v<worm::connection::Client, Client>);
  static_assert(std::is_final_v<Client>);
  static_assert(!std::is_copy_constructible_v<Client>);
  static_assert(!std::is_copy_assignable_v<Client>);
  static_assert(std::is_constructible_v<Client, const worm::connection::ConnectionConfig&>);

  return 0;
}
