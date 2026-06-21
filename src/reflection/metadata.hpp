#pragma once

#include <string_view>

namespace worm
{
  namespace reflection
  {
    struct FieldMetadata
    {
      std::string_view columnName{};
      bool primaryKey = false;
      bool generated = false;
      bool ignored = false;
    };
  } // namespace reflection
} // namespace worm
