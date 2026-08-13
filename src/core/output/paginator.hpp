#pragma once

#include <core/output/result-set.hpp>

#include <cstddef>

namespace worm::core
{
  class Paginator final
  {
    static constexpr std::size_t defaultInitialPage = 1;
    static constexpr std::size_t defaultItemsPerPage = 5;

  public:
    explicit Paginator(ResultSet resultSet,
      std::size_t initialPage = defaultInitialPage,
      std::size_t itemsPerPage = defaultItemsPerPage);

    [[nodiscard]]
    ResultSet paginate();

    Paginator& setCurrentPage(std::size_t page);

    Paginator& setPageSize(std::size_t pageSize);

    Paginator& reset(std::size_t page = defaultInitialPage, std::size_t pageSize = defaultItemsPerPage);

  private:
    [[nodiscard]]
    static std::size_t validatePage(std::size_t page);

    [[nodiscard]]
    static std::size_t validatePageSize(std::size_t pageSize);

    void advancePage() noexcept;

    [[nodiscard]]
    ResultSet paginate(ResultSet::Iterator iterator, const ResultSet::Iterator& end) const;

    ResultSet resultSet_;
    std::size_t page_;
    std::size_t size_;
  };

} // namespace worm::core
