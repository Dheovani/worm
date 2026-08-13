#include <core/output/paginator.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdint>
#include <iostream>
#include <variant>
#include <vector>

namespace
{
  worm::core::ResultRow row(std::int64_t id, bool affected = false)
  {
    return {{{"id", id}}, affected};
  }

  std::int64_t idOf(const worm::core::ResultRow& resultRow)
  {
    return std::get<std::int64_t>(resultRow.columns.front().value);
  }
} // namespace

int main()
{
  const worm::core::ResultSet result{{row(1, true), row(2), row(3, true), row(4), row(5), row(6, true)}};
  worm::core::Paginator paginator{result};

  const worm::core::ResultSet firstPage = paginator.paginate();
  if (firstPage.rowCount() != 5 || idOf(firstPage.rows().front()) != 1 || idOf(firstPage.rows().back()) != 5 ||
      firstPage.affectedRows() != 2) {
    std::cerr << "Paginator did not return the first page correctly.\n";
    return 1;
  }

  const worm::core::ResultSet secondPage = paginator.paginate();
  if (secondPage.rowCount() != 1 || idOf(secondPage.rows().front()) != 6 || secondPage.affectedRows() != 1) {
    std::cerr << "Paginator did not return the partial final page correctly.\n";
    return 1;
  }

  if (!paginator.paginate().empty()) {
    std::cerr << "Paginator did not return an empty result after the final page.\n";
    return 1;
  }

  paginator.reset(2, 2);
  const worm::core::ResultSet customPage = paginator.paginate();
  if (customPage.rowCount() != 2 || idOf(customPage.rows().front()) != 3 || idOf(customPage.rows().back()) != 4) {
    std::cerr << "Paginator did not honor a custom page and page size.\n";
    return 1;
  }

  try {
    paginator.setCurrentPage(0);
    std::cerr << "Paginator accepted page zero.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  try {
    paginator.setPageSize(0);
    std::cerr << "Paginator accepted a zero page size.\n";
    return 1;
  } catch (const worm::InvalidArgException&) {}

  return 0;
}
