#include <utils/logger.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<worm::Logger>);
    static_assert(!std::is_copy_assignable_v<worm::Logger>);

    if (worm::GetClassName("C:\\project\\entity.cpp") != "entity.cpp"
        || worm::GetClassName("/project/entity.cpp") != "entity.cpp"
        || worm::GetClassName("entity.cpp") != "entity.cpp") {
        std::cerr << "GetClassName returned an unexpected value.\n";
        return 1;
    }

    if (worm::GetLogTypeMessage(worm::INFO) != "[INFO] "
        || worm::GetLogTypeMessage(worm::DEBUG) != "[DEBUG] "
        || worm::GetLogTypeMessage(worm::TRACE) != "[TRACE] "
        || worm::GetLogTypeMessage(worm::WARNING) != "[WARNING] "
        || worm::GetLogTypeMessage(worm::ERROR) != "[ERROR] ") {
        std::cerr << "GetLogTypeMessage returned an unexpected value.\n";
        return 1;
    }

    worm::Logger logger("logger-tests.cpp", 42);
    logger.Info("Logger smoke test: %s", "ok");

    return 0;
}
