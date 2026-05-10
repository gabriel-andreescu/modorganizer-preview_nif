if (NOT DEFINED GIT_EXECUTABLE)
    message(FATAL_ERROR "GIT_EXECUTABLE is required")
endif ()

if (NOT DEFINED WORKING_DIR)
    message(FATAL_ERROR "WORKING_DIR is required")
endif ()

if (DEFINED PATCH_FILE)
    list(APPEND PATCH_FILES "${PATCH_FILE}")
endif ()

set(patch_index 1)
while (TRUE)
    set(patch_variable "PATCH_FILE_${patch_index}")
    if (NOT DEFINED ${patch_variable})
        break()
    endif ()

    list(APPEND PATCH_FILES "${${patch_variable}}")
    math(EXPR patch_index "${patch_index} + 1")
endwhile ()

if (NOT DEFINED PATCH_FILES)
    message(FATAL_ERROR "PATCH_FILE or PATCH_FILES is required")
endif ()

get_filename_component(WORKING_DIR "${WORKING_DIR}" ABSOLUTE)

foreach (PATCH_FILE IN LISTS PATCH_FILES)
    get_filename_component(PATCH_FILE "${PATCH_FILE}" ABSOLUTE)

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

        continue()
    endif ()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check --ignore-whitespace "${PATCH_FILE}"
        WORKING_DIRECTORY "${WORKING_DIR}"
        RESULT_VARIABLE patch_already_applied
        OUTPUT_VARIABLE reverse_check_output
        ERROR_VARIABLE reverse_check_error)

    if (patch_already_applied EQUAL 0)
        message(STATUS "Patch already applied: ${PATCH_FILE}")
        continue()
    endif ()

    message(FATAL_ERROR
        "Patch does not apply cleanly and is not already applied: ${PATCH_FILE}\n"
        "Apply check output:\n${patch_check_output}\n${patch_check_error}\n"
        "Reverse check output:\n${reverse_check_output}\n${reverse_check_error}")
endforeach ()
