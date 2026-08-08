#include <utils/dependency-injection.hpp>

#include <errors/missing-configuration-exception.hpp>
#include <errors/unregistered-dependency-exception.hpp>
#include <errors/unsupported-database-exception.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
  struct Value
  {
    int number = 7;
  };

  class ExternalDependency
  {
  public:
    explicit ExternalDependency(int value)
      : value_(value)
    {}

  private:
    int value_;
  };

  void setEnvironment(const char* key, const char* value)
  {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
  }

  void unsetEnvironment(const char* key)
  {
#ifdef _WIN32
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
  }
} // namespace

int main()
{
  const Value value = worm::DependencyInjector<Value>::get();
  if (value.number != 7) {
    std::cerr << "Generic dependency injection returned an invalid value.\n";
    return 1;
  }

  unsetEnvironment("DATABASE_TYPE");
  try {
    static_cast<void>(worm::DependencyInjector<worm::connection::DatabaseType>::get());
    std::cerr << "DatabaseType dependency injection accepted a missing configuration.\n";
    return 1;
  } catch (const worm::MissingConfigurationException&) {}

  setEnvironment("DATABASE_TYPE", "sqlite");
  setEnvironment("HOST", "localhost");
  setEnvironment("USERNAME", "worm");
  setEnvironment("PASSWORD", "secret");
  setEnvironment("DBNAME", ":memory:");
  setEnvironment("PORT", "0");
  setEnvironment("QUERY_CACHE_ENABLED", "true");

  int result = 0;
  try {
    const worm::connection::DatabaseType type = worm::DependencyInjector<worm::connection::DatabaseType>::get();
    const worm::connection::ConnectionConfig config =
      worm::DependencyInjector<worm::connection::ConnectionConfig>::get();

    if (type != worm::connection::DatabaseType::SQLite) {
      std::cerr << "Specialized dependency injection returned an invalid value.\n";
      result = 1;
    }

    if (config.host != "localhost" || config.username != "worm" || config.password != "secret" ||
        config.dbname != ":memory:" || config.port != "0" || !config.cacheResults) {
      std::cerr << "ConnectionConfig dependency injection returned invalid environment values.\n";
      result = 1;
    }

    auto logger = worm::DependencyInjector<worm::Logger>::get<Value, 12>();
    logger.debug("Dependency injection logger smoke test");

    const worm::core::Dialect& dialect = worm::DependencyInjector<worm::core::Dialect>::get();
    const worm::core::SqlBuilder& sqlBuilder = worm::DependencyInjector<worm::core::SqlBuilder>::get();

    if (dynamic_cast<const worm::core::SqliteDialect*>(&dialect) == nullptr ||
        dynamic_cast<const worm::core::SqliteBuilder*>(&sqlBuilder) == nullptr) {
      std::cerr << "Dependency injection did not resolve database-specific abstractions.\n";
      result = 1;
    }

    setEnvironment("DATABASE_TYPE", "mssql");
    const worm::core::Dialect& sqlServerDialect = worm::DependencyInjector<worm::core::Dialect>::get();
    const worm::core::SqlBuilder& sqlServerBuilder = worm::DependencyInjector<worm::core::SqlBuilder>::get();
    if (dynamic_cast<const worm::core::SqlServerDialect*>(&sqlServerDialect) == nullptr ||
        dynamic_cast<const worm::core::SqlServerBuilder*>(&sqlServerBuilder) == nullptr) {
      std::cerr << "Dependency injection did not resolve SQL Server abstractions.\n";
      result = 1;
    }

    setEnvironment("DATABASE_TYPE", "unsupported");
    try {
      static_cast<void>(worm::DependencyInjector<worm::connection::DatabaseType>::get());
      std::cerr << "DatabaseType dependency injection accepted an unsupported database.\n";
      result = 1;
    } catch (const worm::UnsupportedDatabaseException&) {}

    try {
      static_cast<void>(worm::DependencyInjector<ExternalDependency>::get());
      std::cerr << "Dependency injection accepted an unregistered dependency.\n";
      result = 1;
    } catch (const worm::UnregisteredDependencyException&) {}
  } catch (const std::exception& error) {
    std::cerr << "Dependency injection test failed: " << error.what() << '\n';
    result = 1;
  }

  unsetEnvironment("DATABASE_TYPE");
  unsetEnvironment("HOST");
  unsetEnvironment("USERNAME");
  unsetEnvironment("PASSWORD");
  unsetEnvironment("DBNAME");
  unsetEnvironment("PORT");
  unsetEnvironment("QUERY_CACHE_ENABLED");
  return result;
}
