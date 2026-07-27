#include <connection/pg-client.hpp>

#include <type_traits>
#include <variant>
#include <vector>

using worm::connection::PgClient;

namespace
{
  pqxx::params pgParameters(const std::vector<worm::core::Parameter>& parameters)
  {
    pqxx::params values;
    values.reserve(parameters.size());

    for (const worm::core::Parameter& parameter : parameters) {
      std::visit(
        [&values](const auto& value)
        {
          using Value = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<Value, std::nullptr_t>) {
            values.append();
          } else {
            values.append(value);
          }
        },
        parameter);
    }

    return values;
  }
} // namespace

PgClient::PgClient(const std::string& connectionData)
{
  connection_ = new pqxx::connection(connectionData);
}

PgClient::~PgClient()
{
  delete connection_;
}

PgClient& PgClient::getInstance(const worm::connection::ConnectionConfig& databaseConfig)
{
  std::string connectionData =
    " host=" + databaseConfig.host +
    " port=" + databaseConfig.port +
    " dbname=" + databaseConfig.dbname +
    " user=" + databaseConfig.username +
    " password=" + databaseConfig.password;

  static PgClient instance(connectionData);
  return instance;
}

worm::core::ResultSet PgClient::executeQuery(const worm::core::Statement& statement) const
{
  std::vector<worm::core::ResultRow> rows;
  pqxx::work worker = pqxx::work(*connection_);
  pqxx::result response = worker.exec(statement.sql, pgParameters(statement.parameters));
  worker.commit();

  if (isSelect(statement.sql)) {
    for (pqxx::result::size_type i = 0; i < response.size(); i++) {
      std::vector<worm::core::ResultColumn> columns;

      for (pqxx::result::size_type j = 0; j < response[i].size(); j++) {
        const std::string columnName = response[i][j].name();
        worm::core::Parameter columnValue = nullptr;

        if (!response[i][j].is_null()) {
          columnValue = response[i][j].c_str();
        }

        columns.push_back({columnName, columnValue});
      }

      rows.push_back({columns});
    }
  }

  return worm::core::ResultSet{rows};
}
