#pragma once

#include <connection/client.hpp>
#include <core/model/schema-snapshot.hpp>

namespace worm::connection
{
  class SchemaInspector final
  {
  public:
    explicit SchemaInspector(Client& client) noexcept;

    [[nodiscard]]
    core::SchemaSnapshot inspect() const;

  private:
    Client& client_;
  };
} // namespace worm::connection
