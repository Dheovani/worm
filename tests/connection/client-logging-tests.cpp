#include <connection/client.hpp>
#include <core/persistence/repository.hpp>
#include <core/query/sql-builder.hpp>
#include <errors/query-execution-exception.hpp>
#include <reflection/field.hpp>
#include <utils/logger.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>

namespace
{
  struct Record
  {
    std::int64_t id{};

    static constexpr worm::core::Table table() noexcept
    {
      return worm::core::Table{"records"};
    }

    static constexpr worm::core::PrimaryKey primaryKey() noexcept
    {
      return worm::core::PrimaryKey{"pk_records", {worm::core::Column{"id", table()}}};
    }

    static constexpr auto reflect() noexcept
    {
      return std::tuple{worm::reflection::field("id", &Record::id)};
    }
  };

  class LoggingClient final : public worm::connection::Client
  {
  public:
    [[nodiscard]]
    worm::connection::DatabaseType type() const noexcept override
    {
      return worm::connection::DatabaseType::SQLite;
    }

  private:
    void beginTransactionImpl() override {}

    void rollbackTransactionImpl() override {}

    void commitTransactionImpl() override {}

    worm::core::ResultSet executeImpl(const worm::core::Statement& statement) override
    {
      if (statement.sql.find("FAIL") != std::string::npos) {
        throw worm::QueryExecutionException("Native driver rejected parameter 'never-expose-this-value'.");
      }
      return worm::core::ResultSet{{{{{"id", std::int64_t{1}}}}}};
    }
  };

  class LogCapture final
  {
  public:
    LogCapture()
      : previousBuffer_(std::clog.rdbuf(output_.rdbuf())),
        previousLevel_(worm::logger.minimumLevel())
    {
      worm::logger.setMinimumLevel(worm::LogLevel::Trace);
    }

    ~LogCapture()
    {
      worm::logger.setMinimumLevel(previousLevel_);
      std::clog.rdbuf(previousBuffer_);
    }

    [[nodiscard]]
    std::string contents() const
    {
      return output_.str();
    }

  private:
    std::ostringstream output_;
    std::streambuf* previousBuffer_;
    worm::LogLevel previousLevel_;
  };
} // namespace

int main()
{
  std::string diagnostics;
  std::string exceptionMessage;
  {
    LogCapture capture;
    const auto client = std::make_shared<LoggingClient>();
    const worm::core::QueryBuilder queryBuilder{worm::core::SqliteBuilder{}};
    const worm::core::Repository<Record> repository{client, queryBuilder};
    const worm::core::Statement statement{
      "SELECT id FROM records WHERE secret = ?",
      {std::string{"never-log-this-secret"}},
    };

    static_cast<void>(repository.findAll(statement));
    try {
      static_cast<void>(repository.findAll(
        worm::core::Statement{"SELECT id FROM records WHERE secret = ? /* FAIL */",
          {std::string{"never-expose-this-value"}}}));
    } catch (const worm::QueryExecutionException& error) {
      exceptionMessage = error.what();
    }
    diagnostics = capture.contents();
  }

  const bool hasSql = diagnostics.find("SELECT id FROM records WHERE secret = ?") != std::string::npos;
  const bool hasBindingCount = diagnostics.find("binding_count=\"1\"") != std::string::npos;
  const bool hasReturnedRows = diagnostics.find("returned_rows=\"1\"") != std::string::npos;
  const bool loggedParameter = diagnostics.find("never-log-this-secret") != std::string::npos;
  const bool exposedExceptionParameter = exceptionMessage.find("never-expose-this-value") != std::string::npos;
  const bool redactedException = exceptionMessage.find("<redacted>") != std::string::npos;
  if (!hasSql || !hasBindingCount || !hasReturnedRows || loggedParameter || exceptionMessage.empty() ||
      exposedExceptionParameter || !redactedException) {
    std::cerr << "Client SQL diagnostics exposed parameters or omitted safe execution metadata: sql=" << hasSql
              << " bindings=" << hasBindingCount << " rows=" << hasReturnedRows
              << " logged_parameter=" << loggedParameter << " empty_exception=" << exceptionMessage.empty()
              << " exposed_exception_parameter=" << exposedExceptionParameter
              << " redacted_exception=" << redactedException << '\n';
    return 1;
  }

  return 0;
}
