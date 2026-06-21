add_library(
    WormUtils
    client-factory.cpp
    helpers.cpp
)
add_library(Worm::Utils ALIAS WormUtils)

target_compile_features(WormUtils PUBLIC cxx_std_20)

target_include_directories(
    WormUtils
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)

# Public utils headers expose Client, JsonCpp and error types. Worm::Connection
# propagates its external database/JSON dependencies to consumers.
target_link_libraries(
    WormUtils
    PUBLIC
        Worm::Connection
        Worm::Errors
)
