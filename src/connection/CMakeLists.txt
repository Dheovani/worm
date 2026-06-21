add_library(
    WormConnection
    client.cpp
    pg-client.cpp
    mysql-client.cpp
    sqlite-client.cpp
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
        Worm::Dependencies
    PRIVATE
        Worm::Errors
)
