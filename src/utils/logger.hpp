#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <typeinfo>
#include <type_traits>

namespace worm
{
	typedef enum
	{
		INFO,
		DEBUG,
		TRACE,
		WARNING,
		ERROR
	} LogLevel;

	const std::string GetClassName(const char* name)
	{
		std::string fullFilePath(name);
		size_t lastSeparator = fullFilePath.find_last_of("/\\");

		if (lastSeparator == std::string::npos)
		{
			return fullFilePath;
		}

		return fullFilePath.substr(lastSeparator + 1);
	}

	const std::string GetLogTypeMessage(const LogLevel& type)
	{
		switch (type)
		{
		default:
		case INFO:
			return "[INFO] ";

		case DEBUG:
			return "[DEBUG] ";

		case TRACE:
			return "[TRACE] ";

		case WARNING:
			return "[WARNING] ";

		case ERROR:
			return "[ERROR] ";
		}
	}

	class Logger
	{
		int line;
		std::string className;

		// Prevent the use of copy constructor and assignment operator for safety.
		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

	public:
		template <class Class>
		Logger(Class c, int lineNumber)
			: line(lineNumber)
		{
			if constexpr (std::is_same_v<Class, const char*>)
				className = worm::GetClassName(c);
			else
			{
				const std::string name = typeid(c).name();
				size_t pos = name.find(' ', 0) + 1;

				className = (pos == std::string::npos) ? name.c_str() : (name.c_str() + pos);
			}
		}

		template<typename T>
		const Logger& operator<<(const T& value) const
		{
#if defined(_DFLT_LVL_INFO)
			Info(value);
#elif defined(_DFLT_LVL_DEBUG)
			Debug(value);
#elif defined(_DFLT_LVL_TRACE)
			Trace(value);
#elif defined(_DFLT_LVL_WARNING)
			Warning(value);
#else
			Error(value);
#endif // !_DFLT_LVL_
			return *this;
		}

		template<typename... Args>
		inline void Log(const LogLevel& logType, const char* msg, Args... args) const
		{
			std::stringstream ss;
			ss << className;
			if (line)
				ss << ", line: " << line;
			ss << worm::GetLogTypeMessage(logType) << std::endl;

			std::printf(ss.str().c_str());
			std::printf(msg, args...);
			std::cout << std::endl;

#ifdef _LOG_FILE
			const std::string filename = className + "_log_file.log";
			std::fstream fileStream(filename, std::ios::in | std::ios::out | std::ios::trunc);

			if (fileStream.is_open())
				fileStream << GetLogTypeMessage(logType) << msg << std::endl;
			else
				std::cerr << "Failed opening file!" << std::endl;

			fileStream.close();
#endif // _LOG_FILE
		}

		template<typename... Args>
		void Info(const char* msg, Args... args) const
		{
			Log(INFO, msg, args...);
		}

		template<typename... Args>
		void Debug(const char* msg, Args... args) const
		{
			Log(DEBUG, msg, args...);
		}

		template<typename... Args>
		void Trace(const char* msg, Args... args) const
		{
			Log(TRACE, msg, args...);
		}

		template<typename... Args>
		void Warning(const char* msg, Args... args) const
		{
			Log(WARNING, msg, args...);
		}

		template<typename... Args>
		void Error(const char* msg, Args... args) const
		{
			Log(ERROR, msg, args...);
		}

		template<typename... Args>
		void Error(const std::exception& ex, Args... args) const
		{
			Log(ERROR, ex.what(), args...);
		}
	};
}

#define LOGGER \
	worm::utils::Logger(__FILE__, __LINE__)
