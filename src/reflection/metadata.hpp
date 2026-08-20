#pragma once

#include <string_view>

namespace worm::reflection
{
  struct FieldMetadata
  {
    std::string_view columnName{};
    bool generated = false;
    bool ignored = false;
    bool unique = false;
    bool nullable = true;
  };
} // namespace worm::reflection
