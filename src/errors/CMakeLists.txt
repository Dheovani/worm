add_library(WormErrors INTERFACE)
add_library(Worm::Errors ALIAS WormErrors)

set_target_properties(WormErrors PROPERTIES EXPORT_NAME Errors)

target_include_directories(
  WormErrors
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)
