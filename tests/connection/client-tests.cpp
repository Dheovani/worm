#include <connection/client.hpp>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    class TestClient final : public worm::connection::Client
    {
    public:
        bool IsSelect(const std::string& query) const
        {
            return isSelect(query);
        }

        const Json::Value executeQuery(const std::string&) const override
        {
            return {};
        }
    };
}

int main()
{
    const TestClient client;
    const std::vector<std::pair<std::string, bool>> cases = {
        { "SELECT * FROM users", true },
        { "select id from users", true },
        { "  SeLeCt 1", true },
        { "INSERT INTO users VALUES (1)", false },
        { "WITH users AS (SELECT 1) SELECT * FROM users", false },
        { "", false },
    };

    for (const auto& [query, expected] : cases) {
        if (client.IsSelect(query) == expected)
            continue;

        std::cerr << "Unexpected SELECT detection for query: " << query << '\n';
        return 1;
    }

    if (worm::types.at("postgresql") != worm::PostgreSQL
        || worm::types.at("mysql") != worm::MySQL
        || worm::types.at("sqlite") != worm::SQLite) {
        std::cerr << "Database type mapping is incorrect.\n";
        return 1;
    }

    return 0;
}
