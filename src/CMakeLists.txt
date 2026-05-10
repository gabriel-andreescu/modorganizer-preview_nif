cmake_minimum_required(VERSION 3.22)

find_package(mo2-cmake CONFIG REQUIRED)
find_package(mo2-uibase CONFIG REQUIRED)
find_package(mo2-dds-header CONFIG REQUIRED)
find_package(mo2-libbsarch CONFIG REQUIRED)
find_package(Qt6 COMPONENTS OpenGLWidgets REQUIRED)

function(preview_nif_fix_qt_tool_locations)
    get_filename_component(qt_install_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    foreach (qt_tool_name
            androiddeployqt
            androidtestrunner
            cmake_automoc_parser
            lconvert
            lrelease
            lupdate
            moc
            qlalr
            qmake
            qmlcachegen
            qmldom
            qmlformat
            qmlimportscanner
            qmljsrootgen
            qmllint
            qmlplugindump
            qmlprofiler
            qmltc
            qmltestrunner
            qmltime
            qmltyperegistrar
            qtpaths
            qvkgen
            rcc
            svgtoqml
            syncqt
            tracegen
            tracepointgen
            uic
            windeployqt)
        set(qt_tool_exe "${qt_install_prefix}/bin/${qt_tool_name}.exe")
        if (EXISTS "${qt_tool_exe}")
            foreach (qt_tool_namespace Qt Qt6)
                if (TARGET ${qt_tool_namespace}::${qt_tool_name})
                    set_target_properties(${qt_tool_namespace}::${qt_tool_name} PROPERTIES
                        IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
                        IMPORTED_LOCATION "${qt_tool_exe}"
                        IMPORTED_LOCATION_DEBUG "${qt_tool_exe}"
                        IMPORTED_LOCATION_RELEASE "${qt_tool_exe}"
                        IMPORTED_LOCATION_RELWITHDEBINFO "${qt_tool_exe}"
                        IMPORTED_LOCATION_MINSIZEREL "${qt_tool_exe}")
                endif ()
            endforeach ()
        endif ()
    endforeach ()
endfunction()

preview_nif_fix_qt_tool_locations()

add_library(preview_nif SHARED)
mo2_configure_plugin(preview_nif WARNINGS OFF)
preview_nif_fix_qt_tool_locations()
set(MO2_UIBASE_INCLUDE_DIR "" CACHE PATH
    "Optional uibase include directory to build against a specific MO2 ABI")
if (MO2_UIBASE_INCLUDE_DIR)
    message(STATUS "[preview_nif] Using MO2 uibase ABI headers: ${MO2_UIBASE_INCLUDE_DIR}")
    target_include_directories(preview_nif SYSTEM BEFORE PRIVATE "${MO2_UIBASE_INCLUDE_DIR}")
endif ()
target_link_libraries(preview_nif PRIVATE Qt6::OpenGLWidgets mo2::libbsarch mo2::uibase)
mo2_install_plugin(preview_nif)
preview_nif_fix_qt_tool_locations()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../data"
        DESTINATION bin/plugins)

set(PREVIEW_NIF_PACKAGE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dist" CACHE PATH
    "Directory where release ZIP files are written")
set(PREVIEW_NIF_PACKAGE_MO2_VERSION "" CACHE STRING
    "MO2 compatibility label appended to the release ZIP name")

set(preview_nif_package_name "NIF.Preview")
if (PREVIEW_NIF_PACKAGE_MO2_VERSION)
    string(APPEND preview_nif_package_name ".${PREVIEW_NIF_PACKAGE_MO2_VERSION}")
endif ()
string(APPEND preview_nif_package_name ".zip")

add_custom_target(preview_nif_zip
    COMMAND "${CMAKE_COMMAND}"
        "-DOUTPUT_ZIP=${PREVIEW_NIF_PACKAGE_OUTPUT_DIR}/${preview_nif_package_name}"
        "-DPLUGIN_DLL=$<TARGET_FILE:preview_nif>"
        "-DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../data"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/preview_nif_zip"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/package_preview_nif.cmake"
    DEPENDS preview_nif
    COMMENT "Creating ${PREVIEW_NIF_PACKAGE_OUTPUT_DIR}/${preview_nif_package_name}")
