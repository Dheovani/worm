#pragma once

#include <connection/client.hpp>
#include <core/model/schema-snapshot.hpp>

#include <memory>

namespace worm::connection
{
  class SchemaInspector final
  {
  public:
    explicit SchemaInspector(Client& client) noexcept;
    explicit SchemaInspector(std::unique_ptr<Client> client) noexcept;

    [[nodiscard]]
    core::SchemaSnapshot inspect() const;

  private:
    std::unique_ptr<Client> ownedClient_;
    Client* client_;
  };
} // namespace worm::connection
