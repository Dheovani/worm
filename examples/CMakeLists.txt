if(NOT WORM_ENABLE_SQLITE)
  message(FATAL_ERROR "WORM_BUILD_EXAMPLES requires WORM_ENABLE_SQLITE=ON.")
endif()

add_executable(WormSqliteQuickStart sqlite-quick-start.cpp)
add_executable(Worm::SqliteQuickStart ALIAS WormSqliteQuickStart)

target_compile_features(WormSqliteQuickStart PRIVATE cxx_std_20)
target_link_libraries(
  WormSqliteQuickStart
  PRIVATE
    Worm::Connection
    Worm::Core
)
