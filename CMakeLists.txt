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
