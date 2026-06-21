#include <utils/helpers.hpp>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <errors/invalid-arg-exception.hpp>

const std::string utils::env::FindEnvInProjectRoot()
{
    std::filesystem::path current = std::filesystem::current_path();

    while (!current.empty()) {
        std::filesystem::path env_path = current / ".env";

        if (std::filesystem::exists(env_path))
            return env_path.string();
        else if (current.has_parent_path())
            current = current.parent_path();
        else
            break;
    }

    return "";
}

const std::unordered_map<std::string, std::string> utils::env::LoadFromPath(const std::string& path)
{
    std::unordered_map<std::string, std::string> vars;
    std::ifstream file(path);

    if (!file.is_open())
        throw worm::InvalidArgException(("[EnvLoader] Não foi possível abrir o arquivo: " + path));

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);

        vars[key] = value;
#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }

    return vars;
}

const std::string utils::env::GetDatabaseType()
{
    std::string path = FindEnvInProjectRoot();
    
    if (path.empty())
        throw worm::InvalidArgException("Path to .env not found. Please configure your .env file in the root directory.");

    std::unordered_map<std::string, std::string> env_vars = LoadFromPath(path);
    std::string db = env_vars.at("database_type");

    if (db.empty())
        db = std::getenv("database_type");

    return db;
}
