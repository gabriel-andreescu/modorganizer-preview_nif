if (NOT DEFINED OUTPUT_ZIP OR OUTPUT_ZIP STREQUAL "")
    message(FATAL_ERROR "OUTPUT_ZIP is required")
endif ()

if (NOT DEFINED PLUGIN_DLL OR NOT EXISTS "${PLUGIN_DLL}")
    message(FATAL_ERROR "PLUGIN_DLL does not exist: ${PLUGIN_DLL}")
endif ()

if (NOT DEFINED DATA_DIR OR NOT EXISTS "${DATA_DIR}")
    message(FATAL_ERROR "DATA_DIR does not exist: ${DATA_DIR}")
endif ()

if (NOT DEFINED WORK_DIR OR WORK_DIR STREQUAL "")
    message(FATAL_ERROR "WORK_DIR is required")
endif ()

get_filename_component(output_dir "${OUTPUT_ZIP}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY "${PLUGIN_DLL}" DESTINATION "${WORK_DIR}")
file(COPY "${DATA_DIR}" DESTINATION "${WORK_DIR}")
file(REMOVE "${OUTPUT_ZIP}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT_ZIP}" --format=zip
            preview_nif.dll data
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE zip_result)

if (NOT zip_result EQUAL 0)
    message(FATAL_ERROR "Failed to create ${OUTPUT_ZIP}")
endif ()

message(STATUS "Created ${OUTPUT_ZIP}")
