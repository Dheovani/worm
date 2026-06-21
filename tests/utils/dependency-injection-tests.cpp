#include <utils/dependency-injection.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
  struct Value
  {
    int number = 7;
  };
} // namespace

int main()
{
  const Value value = worm::DependencyInjector<Value>().get();
  if (value.number != 7) {
    std::cerr << "Generic dependency injection returned an invalid value.\n";
    return 1;
  }

  const std::filesystem::path originalPath = std::filesystem::current_path();
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "worm-dependency-injection-tests";

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  {
    std::ofstream envFile(root / ".env");
    envFile << "database_type=sqlite\n"
            << "dbname=:memory:\n";
  }

  std::filesystem::current_path(root);

  int result = 0;
  try {
    const worm::DatabaseType type = worm::DependencyInjector<worm::DatabaseType>().get();

    if (type != worm::DatabaseType::SQLite) {
      std::cerr << "Specialized dependency injection returned an invalid value.\n";
      result = 1;
    }

    auto logger = worm::DependencyInjector<worm::Logger>().get<Value, 12>();
    logger.debug("Dependency injection logger smoke test");
  } catch (const std::exception& error) {
    std::cerr << "Dependency injection test failed: " << error.what() << '\n';
    result = 1;
  }

  std::filesystem::current_path(originalPath);
  std::filesystem::remove_all(root);
  return result;
}
