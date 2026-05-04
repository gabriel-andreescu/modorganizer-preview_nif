cmake_minimum_required(VERSION 3.22)

find_package(mo2-cmake CONFIG REQUIRED)
find_package(mo2-uibase CONFIG REQUIRED)
find_package(mo2-libbsarch CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS OpenGL OpenGLWidgets)

add_library(preview_nif SHARED)
mo2_configure_plugin(
        preview_nif
        WARNINGS OFF
)
target_link_libraries(preview_nif PRIVATE
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        mo2::libbsarch
        mo2::uibase
)
mo2_install_plugin(preview_nif)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../data"
        DESTINATION ${MO2_INSTALL_BIN}/plugins)
