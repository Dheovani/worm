#pragma once

#include <string_view>

namespace worm::reflection
{
  struct FieldMetadata
  {
    std::string_view columnName{};
    bool primaryKey = false;
    bool generated = false;
    bool ignored = false;
  };
} // namespace worm::reflection
