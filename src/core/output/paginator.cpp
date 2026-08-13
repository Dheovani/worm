#include <core/output/paginator.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace worm::core
{

  Paginator::Paginator(ResultSet resultSet, std::size_t initialPage, std::size_t itemsPerPage)
    : resultSet_(std::move(resultSet)),
      page_(validatePage(initialPage)),
      size_(validatePageSize(itemsPerPage))
  {}

  ResultSet Paginator::paginate()
  {
    const std::size_t rowCount = resultSet_.rowCount();
    const std::size_t pageIndex = page_ - 1;
    if (rowCount == 0 || pageIndex > rowCount / size_) {
      advancePage();
      return {};
    }

    const std::size_t beginIndex = pageIndex * size_;
    const std::size_t pageLength = (std::min)(size_, rowCount - beginIndex);
    const std::size_t endIndex = beginIndex + pageLength;
    ResultSet::Iterator iterator = resultSet_.begin() + static_cast<std::ptrdiff_t>(beginIndex);
    const ResultSet::Iterator end = resultSet_.begin() + static_cast<std::ptrdiff_t>(endIndex);

    ResultSet pageResult = paginate(iterator, end);
    advancePage();
    return pageResult;
  }

  Paginator& Paginator::setCurrentPage(std::size_t page)
  {
    page_ = validatePage(page);
    return *this;
  }

  Paginator& Paginator::setPageSize(std::size_t pageSize)
  {
    size_ = validatePageSize(pageSize);
    return *this;
  }

  Paginator& Paginator::reset(std::size_t page, std::size_t pageSize)
  {
    page_ = validatePage(page);
    size_ = validatePageSize(pageSize);
    return *this;
  }

  std::size_t Paginator::validatePage(std::size_t page)
  {
    if (page == 0) {
      throw InvalidArgException("Paginator page numbers start at 1.");
    }

    return page;
  }

  std::size_t Paginator::validatePageSize(std::size_t pageSize)
  {
    if (pageSize == 0) {
      throw InvalidArgException("Paginator page size must be greater than zero.");
    }

    return pageSize;
  }

  void Paginator::advancePage() noexcept
  {
    if (page_ < (std::numeric_limits<std::size_t>::max)()) {
      ++page_;
    }
  }

  ResultSet Paginator::paginate(ResultSet::Iterator iterator, const ResultSet::Iterator& end) const
  {
    std::uint64_t affectedRows = 0;
    std::vector<ResultRow> rows;

    while (iterator != end) {
      rows.push_back(*iterator);
      if (iterator->affected) {
        ++affectedRows;
      }
      ++iterator;
    }

    return ResultSet{std::move(rows), affectedRows};
  }

} // namespace worm::core
