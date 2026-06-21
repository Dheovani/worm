#pragma once

#include <exception>
#include <string>

namespace worm
{
	class DatabaseException: public std::exception
	{
	private:
		std::string errorMessage;

	public:
		DatabaseException(const std::string& msg) : errorMessage(msg)
		{}

		const char* what() const noexcept override
		{
			return errorMessage.c_str();
		}
	};
}
