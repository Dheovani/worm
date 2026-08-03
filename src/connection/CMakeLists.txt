set(
  WORM_CONNECTION_SOURCES
  configuration.cpp
  transaction.cpp
)

if(WORM_ENABLE_POSTGRESQL)
  list(APPEND WORM_CONNECTION_SOURCES drivers/pg-client.cpp)
endif()
if(WORM_ENABLE_MYSQL)
  list(APPEND WORM_CONNECTION_SOURCES drivers/mysql-client.cpp)
endif()
if(WORM_ENABLE_SQLITE)
  list(APPEND WORM_CONNECTION_SOURCES drivers/sqlite-client.cpp)
endif()

add_library(WormConnection ${WORM_CONNECTION_SOURCES})
add_library(Worm::Connection ALIAS WormConnection)

target_include_directories(
  WormConnection
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

target_link_libraries(
  WormConnection
  PUBLIC
    Worm::Core
    Worm::Dependencies
  PRIVATE
    Worm::Errors
)

target_compile_definitions(
  WormConnection
  PUBLIC
    $<$<BOOL:${WORM_ENABLE_POSTGRESQL}>:WORM_HAS_POSTGRESQL_DRIVER=1>
    $<$<BOOL:${WORM_ENABLE_MYSQL}>:WORM_HAS_MYSQL_DRIVER=1>
    $<$<BOOL:${WORM_ENABLE_SQLITE}>:WORM_HAS_SQLITE_DRIVER=1>
)
