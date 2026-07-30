add_library(
  WormConnection
  configuration.cpp
  pg-client.cpp
  mysql-client.cpp
  sqlite-client.cpp
  transaction.cpp
)
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
