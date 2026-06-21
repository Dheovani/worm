add_library(WormErrors INTERFACE)
add_library(Worm::Errors ALIAS WormErrors)

target_include_directories(
    WormErrors
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
)
