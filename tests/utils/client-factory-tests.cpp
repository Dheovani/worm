#include <utils/client-factory.hpp>

#include <connection/sqlite-client.hpp>
#include <errors/database-exception.hpp>

#include <iostream>

int main()
{
    Json::Value config;
    config["dbname"] = ":memory:";

    worm::connection::Client& client = worm::GetInstance(config, worm::SQLite);
    if (dynamic_cast<worm::connection::SqLiteClient*>(&client) == nullptr) {
        std::cerr << "Client factory returned the wrong SQLite client type.\n";
        return 1;
    }

    try {
        worm::GetInstance(config, static_cast<worm::DbTypes>(999));
    }
    catch (const worm::DatabaseException&) {
        return 0;
    }
    catch (...) {
        std::cerr << "Client factory threw an unexpected error type.\n";
        return 1;
    }

    std::cerr << "Client factory accepted an unsupported database type.\n";
    return 1;
}
