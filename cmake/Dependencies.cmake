include_guard(GLOBAL)

# JsonCpp and libpqxx provide stable config-package targets both upstream and
# through vcpkg.
find_package(jsoncpp CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)

if(TARGET JsonCpp::JsonCpp)
    set(WORM_JSONCPP_TARGET JsonCpp::JsonCpp)
elseif(TARGET jsoncpp_lib)
    set(WORM_JSONCPP_TARGET jsoncpp_lib)
else()
    message(FATAL_ERROR "JsonCpp was found, but it did not export a supported CMake target.")
endif()

if(NOT TARGET libpqxx::pqxx)
    message(FATAL_ERROR "libpqxx was found, but target libpqxx::pqxx is unavailable.")
endif()

# vcpkg's SQLite port exports an unofficial config target. Keep the built-in
# FindSQLite3 module as a fallback for system installations.
find_package(unofficial-sqlite3 CONFIG QUIET)
if(TARGET unofficial::sqlite3::sqlite3)
    set(WORM_SQLITE_TARGET unofficial::sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(WORM_SQLITE_TARGET SQLite::SQLite3)
endif()

# vcpkg's libmysql port exports unofficial::libmysql::libmysql. On Unix-like
# systems, also accept a mysqlclient installation discoverable by pkg-config.
find_package(unofficial-libmysql CONFIG QUIET)
if(TARGET unofficial::libmysql::libmysql)
    set(WORM_MYSQL_TARGET unofficial::libmysql::libmysql)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(MySQLClient QUIET IMPORTED_TARGET mysqlclient)
    endif()

    if(TARGET PkgConfig::MySQLClient)
        set(WORM_MYSQL_TARGET PkgConfig::MySQLClient)
    else()
        message(
            FATAL_ERROR
            "MySQL client library not found. Use the vcpkg manifest included "
            "with Worm or install mysqlclient so it is visible to pkg-config."
        )
    endif()
endif()

# One normalized target keeps third-party details out of the project modules.
add_library(WormDependencies INTERFACE)
add_library(Worm::Dependencies ALIAS WormDependencies)

target_link_libraries(
    WormDependencies
    INTERFACE
        ${WORM_JSONCPP_TARGET}
        libpqxx::pqxx
        ${WORM_SQLITE_TARGET}
        ${WORM_MYSQL_TARGET}
)
