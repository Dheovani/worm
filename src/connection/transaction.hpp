#pragma once

namespace worm::connection
{
  class Client;

  class Transaction
  {
    friend class Client;

  public:
    ~Transaction() noexcept;

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;

    void commit();

    void rollback();

    [[nodiscard]]
    bool active() const noexcept;

  private:
    explicit Transaction(Client& client);

    void ensureActive() const;

    Client* client_;
    bool active_;
  };

} // namespace worm::connection
