#include <connection/sqlite-client.hpp>

#include <iostream>
#include <string>
#include <type_traits>

int main()
{
  using Client = worm::connection::SqliteClient;

  static_assert(std::is_base_of_v<worm::connection::Client, Client>);
  static_assert(std::is_final_v<Client>);
  static_assert(!std::is_copy_constructible_v<Client>);
  static_assert(!std::is_copy_assignable_v<Client>);

  Json::Value config;
  config["dbname"] = ":memory:";

  Client& client = Client::getInstance(config);
  Client& sameClient = Client::getInstance(config);

  if (&client != &sameClient) {
    std::cerr << "SqliteClient did not preserve its singleton instance.\n";
    return 1;
  }

  const Json::Value response = client.executeQuery("SELECT 42 AS answer, NULL AS missing");

  const Json::Value& rows = response["results"];
  if (!rows.isArray() || rows.size() != 1) {
    std::cerr << "SqliteClient returned an unexpected row count.\n";
    return 1;
  }

  if (rows[0]["answer"].asString() != "42" || rows[0]["missing"].asString() != "") {
    std::cerr << "SqliteClient returned unexpected column values.\n";
    return 1;
  }

  return 0;
}
