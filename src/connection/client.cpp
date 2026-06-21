#include <string>
#include <algorithm>
#include <connection/client.hpp>
#include <errors/database-exception.hpp>

using worm::connection::Client;

bool Client::isSelect(const std::string& query) const
{
	std::istringstream iss(query);
	std::string firstWord;

	iss >> firstWord;
	std::transform(firstWord.begin(), firstWord.end(), firstWord.begin(), ::toupper);

	return firstWord == "SELECT";
}
