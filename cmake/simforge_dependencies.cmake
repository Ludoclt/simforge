include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG v0.9.5
)
FetchContent_MakeAvailable(magic_enum)

add_subdirectory(libs/CLI11 EXCLUDE_FROM_ALL)
add_subdirectory(libs/json EXCLUDE_FROM_ALL)
add_subdirectory(libs/tomlplusplus EXCLUDE_FROM_ALL)
