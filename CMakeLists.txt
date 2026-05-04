cmake_minimum_required(VERSION 3.22)

project(preview_nif)

FetchContent_Declare(
        gli
        GIT_REPOSITORY https://github.com/g-truc/gli.git
        GIT_TAG master
)
set(GLI_TEST_ENABLE OFF CACHE BOOL "Build gli unit tests")

FetchContent_Declare(
        nifly
        GIT_REPOSITORY https://github.com/ousnius/nifly.git
        GIT_TAG main
)

FetchContent_MakeAvailable(gli nifly)

add_subdirectory(src)
target_link_libraries(preview_nif PRIVATE nifly gli)
