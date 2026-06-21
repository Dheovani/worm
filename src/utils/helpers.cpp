#include <utils/helpers.hpp>

#include <errors/invalid-arg-exception.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>

std::string worm::utils::env::findInProjectRoot()
{
  std::filesystem::path current = std::filesystem::current_path();

  while (!current.empty()) {
    const std::filesystem::path envPath = current / ".env";

    if (std::filesystem::exists(envPath))
      return envPath.string();
    if (current.has_parent_path())
      current = current.parent_path();
    else
      break;
  }

  return {};
}

std::unordered_map<std::string, std::string> worm::utils::env::loadFromPath(const std::string& path)
{
  std::unordered_map<std::string, std::string> variables;
  std::ifstream file(path);

  if (!file.is_open())
    throw InvalidArgException("Unable to open environment file: " + path);

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line.front() == '#')
      continue;

    const auto separator = line.find('=');
    if (separator == std::string::npos)
      continue;

    std::string key = line.substr(0, separator);
    std::string value = line.substr(separator + 1);

    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t\""));
    value.erase(value.find_last_not_of(" \t\"") + 1);

    variables[key] = value;
#ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 1);
#endif
  }

  return variables;
}

std::string worm::utils::env::getDatabaseType()
{
  const std::string path = findInProjectRoot();
  if (path.empty())
    throw InvalidArgException("The project .env file was not found.");

  const auto variables = loadFromPath(path);
  const auto databaseType = variables.find("database_type");
  if (databaseType == variables.end() || databaseType->second.empty())
    throw InvalidArgException("The database_type environment variable is missing.");

  return databaseType->second;
}
