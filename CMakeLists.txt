cmake_minimum_required(VERSION 3.22)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file")
endif ()

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "vcpkg target triplet")
set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON CACHE BOOL "Use only default vcpkg manifest dependencies")

set(PREVIEW_NIF_RELEASE_VERSION "0.4.4" CACHE STRING "NIF Preview release version")

project(preview_nif VERSION ${PREVIEW_NIF_RELEASE_VERSION})

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../../install"
        CACHE PATH "Install prefix" FORCE)
endif ()

foreach (prefix
        "${CMAKE_CURRENT_LIST_DIR}/../../cmake_common"
        "${CMAKE_CURRENT_LIST_DIR}/../../../install/lib/cmake"
        "C:/Qt/6.7.3/msvc2019_64")
    if (EXISTS "${prefix}")
        list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    endif ()
endforeach ()
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE STRING "Additional package prefixes")

include(FetchContent)
find_package(Git REQUIRED)

set(gli_clang22_enum_bitmask_patch
        "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/gli-clang22-enum-bitmask.patch")
set(gli_clang_tidy_texture_cache_patch
        "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/gli-clang-tidy-texture-cache.patch")

FetchContent_Declare(
        gli
        GIT_REPOSITORY https://github.com/g-truc/gli.git
        GIT_TAG 3542f8830178061e0661f5df1e89d36ba7d7b0ab
        PATCH_COMMAND "${CMAKE_COMMAND}"
                      "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                      "-DWORKING_DIR=<SOURCE_DIR>"
                      "-DPATCH_FILE=${gli_clang22_enum_bitmask_patch}"
                      "-DPATCH_FILE_1=${gli_clang_tidy_texture_cache_patch}"
                      -P "${CMAKE_CURRENT_LIST_DIR}/cmake/apply_fetchcontent_patch.cmake"
)
set(GLI_TEST_ENABLE OFF CACHE BOOL "Build gli unit tests")

FetchContent_Declare(
        nifly
        GIT_REPOSITORY https://github.com/ousnius/nifly.git
        GIT_TAG 77bb1b2373d9fd6147945eca2ff6ba22a281da80
        PATCH_COMMAND "${CMAKE_COMMAND}"
                      "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                      "-DWORKING_DIR=<SOURCE_DIR>"
                      "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/cmake/patches/nifly-unionbv-complete-type.patch"
                      -P "${CMAKE_CURRENT_LIST_DIR}/cmake/apply_fetchcontent_patch.cmake"
)

FetchContent_MakeAvailable(gli nifly)

add_subdirectory(src)
target_link_libraries(preview_nif PRIVATE nifly gli)
