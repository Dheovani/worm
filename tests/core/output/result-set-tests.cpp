#include <core/output/result-set.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

int main()
{
  const worm::core::ResultSet emptyResult;
  if (!emptyResult.empty() || emptyResult.rowCount() != 0 || emptyResult.begin() != emptyResult.end()) {
    std::cerr << "Empty ResultSet did not expose an empty container contract.\n";
    return 1;
  }

  const worm::core::ResultSet result{
    {
      worm::core::ResultRow{
        {
          worm::core::ResultColumn{"id", std::int64_t{7}},
          worm::core::ResultColumn{"name", std::string{"Ada"}},
          worm::core::ResultColumn{"deleted_at", nullptr},
        }},
    }};

  if (result.empty() || result.rowCount() != 1) {
    std::cerr << "ResultSet did not preserve rows.\n";
    return 1;
  }

  const auto& row = result.rows().front();
  if (row.empty() || row.columnCount() != 3) {
    std::cerr << "ResultRow did not preserve columns.\n";
    return 1;
  }

  if (row.columns[0].name != "id" || std::get<std::int64_t>(row.columns[0].value) != 7 ||
      row.columns[1].name != "name" || std::get<std::string>(row.columns[1].value) != "Ada" ||
      row.columns[2].name != "deleted_at" || !std::holds_alternative<std::nullptr_t>(row.columns[2].value)) {
    std::cerr << "ResultSet did not preserve typed column values.\n";
    return 1;
  }

  return 0;
}
