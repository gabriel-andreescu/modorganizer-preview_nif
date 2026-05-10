if (NOT DEFINED GIT_EXECUTABLE)
    message(FATAL_ERROR "GIT_EXECUTABLE is required")
endif ()

if (NOT DEFINED WORKING_DIR)
    message(FATAL_ERROR "WORKING_DIR is required")
endif ()

if (NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "PATCH_FILE is required")
endif ()

get_filename_component(PATCH_FILE "${PATCH_FILE}" ABSOLUTE)
get_filename_component(WORKING_DIR "${WORKING_DIR}" ABSOLUTE)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --ignore-whitespace "${PATCH_FILE}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE patch_applies
    OUTPUT_VARIABLE patch_check_output
    ERROR_VARIABLE patch_check_error)

if (patch_applies EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${PATCH_FILE}"
        WORKING_DIRECTORY "${WORKING_DIR}"
        RESULT_VARIABLE patch_result
        OUTPUT_VARIABLE patch_output
        ERROR_VARIABLE patch_error)

    if (NOT patch_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to apply ${PATCH_FILE}\n${patch_output}\n${patch_error}")
    endif ()

    return()
endif ()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --ignore-whitespace "${PATCH_FILE}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE patch_already_applied
    OUTPUT_VARIABLE reverse_check_output
    ERROR_VARIABLE reverse_check_error)

if (patch_already_applied EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH_FILE}")
    return()
endif ()

message(FATAL_ERROR
    "Patch does not apply cleanly and is not already applied: ${PATCH_FILE}\n"
    "Apply check output:\n${patch_check_output}\n${patch_check_error}\n"
    "Reverse check output:\n${reverse_check_output}\n${reverse_check_error}")
