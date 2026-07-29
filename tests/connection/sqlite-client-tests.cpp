#include <connection/sqlite-client.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>

int main()
{
  using Client = worm::connection::SqliteClient;

  static_assert(std::is_base_of_v<worm::connection::Client, Client>);
  static_assert(std::is_final_v<Client>);
  static_assert(!std::is_copy_constructible_v<Client>);
  static_assert(!std::is_copy_assignable_v<Client>);

  const worm::connection::ConnectionConfig config{
    .dbname = ":memory:",
  };

  Client& client = Client::getInstance(config);
  Client& sameClient = Client::getInstance(config);

  if (&client != &sameClient) {
    std::cerr << "SqliteClient did not preserve its singleton instance.\n";
    return 1;
  }

  const worm::core::ResultSet response = client.executeQuery({"SELECT 42 AS answer, NULL AS missing"});

  if (response.rowCount() != 1) {
    std::cerr << "SqliteClient returned an unexpected row count.\n";
    return 1;
  }

  const auto& row = response.rows().front();
  if (row.columnCount() != 2) {
    std::cerr << "SqliteClient returned an unexpected column count.\n";
    return 1;
  }

  if (row.columns[0].name != "answer" || std::get<std::int64_t>(row.columns[0].value) != 42 ||
      row.columns[1].name != "missing" || !std::holds_alternative<std::nullptr_t>(row.columns[1].value)) {
    std::cerr << "SqliteClient returned unexpected column values.\n";
    return 1;
  }

  const worm::core::ResultSet createResponse =
    client.executeQuery({"CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT NOT NULL, active INTEGER NOT NULL)"});
  const worm::core::ResultSet insertAdaResponse =
    client.executeQuery({"INSERT INTO people (name, active) VALUES (?, ?)", {std::string{"Ada"}, true}});
  const worm::core::ResultSet insertGraceResponse =
    client.executeQuery({"INSERT INTO people (name, active) VALUES (?, ?)", {std::string{"Grace"}, false}});

  if (!createResponse.empty() ||
      createResponse.affectedRows() != 0 ||
      !insertAdaResponse.empty() ||
      insertAdaResponse.affectedRows() != 1 ||
      !insertGraceResponse.empty() ||
      insertGraceResponse.affectedRows() != 1) {
    std::cerr << "SqliteClient did not report command feedback correctly.\n";
    return 1;
  }

  const worm::core::ResultSet filteredResponse =
    client.executeQuery({"SELECT name, active FROM people WHERE active = ?", {true}});

  if (filteredResponse.rowCount() != 1) {
    std::cerr << "SqliteClient did not bind parameters in the filtered query.\n";
    return 1;
  }

  const auto& filteredRow = filteredResponse.rows().front();
  if (std::get<std::string>(filteredRow.columns[0].value) != "Ada" ||
      std::get<std::int64_t>(filteredRow.columns[1].value) != 1) {
    std::cerr << "SqliteClient returned unexpected values for a parameterized query.\n";
    return 1;
  }

  const worm::core::ResultSet updateResponse =
    client.executeQuery({"UPDATE people SET active = ? WHERE name = ?", {true, std::string{"Grace"}}});

  if (!updateResponse.empty() || updateResponse.affectedRows() != 1) {
    std::cerr << "SqliteClient did not report update feedback correctly.\n";
    return 1;
  }

  const worm::core::ResultSet deleteResponse =
    client.executeQuery({"DELETE FROM people WHERE name = ?", {std::string{"Grace"}}});

  if (!deleteResponse.empty() || deleteResponse.affectedRows() != 1) {
    std::cerr << "SqliteClient did not report delete feedback correctly.\n";
    return 1;
  }

  return 0;
}
