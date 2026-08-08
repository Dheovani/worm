#include <context/cache.hpp>

#include <iostream>
#include <string>

int main()
{
  worm::context::Cache<std::string, int> cache;

  if (!cache.empty() || cache.size() != 0 || cache.contains("missing") || cache.get("missing").has_value()) {
    std::cerr << "A new cache did not expose an empty state.\n";
    return 1;
  }

  cache.add("answer", 41).add("answer", 42);
  const auto answer = cache.get("answer");
  if (!answer.has_value() || answer->get() != 42 || !cache.contains("answer") || cache.size() != 1) {
    std::cerr << "Cache did not insert or replace a value by key.\n";
    return 1;
  }

  cache.clear();
  if (!cache.empty() || cache.contains("answer")) {
    std::cerr << "Cache did not clear its entries.\n";
    return 1;
  }

  return 0;
}
