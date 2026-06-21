#pragma once

#include <exception>
#include <string>

namespace worm
{
	class InvalidArgException: public std::exception
	{
	private:
		std::string errorMessage;

	public:
		InvalidArgException(const std::string& msg) : errorMessage(msg)
		{}

		const char* what() const noexcept override
		{
			return errorMessage.c_str();
		}
	};
}
