#include <algorithm>
#include <cctype>
#include <connection/client.hpp>
#include <errors/database-exception.hpp>
#include <sstream>
#include <string>

using worm::connection::Client;

bool Client::isSelect(const std::string& query) const
{
  std::istringstream iss(query);
  std::string firstWord;

  iss >> firstWord;
  std::transform(firstWord.begin(), firstWord.end(), firstWord.begin(),
                 [](unsigned char value) { return static_cast<char>(std::toupper(value)); });

  return firstWord == "SELECT";
}
