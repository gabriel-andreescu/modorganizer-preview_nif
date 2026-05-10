if (NOT DEFINED CLANG_TIDY_EXECUTABLE)
    set(CLANG_TIDY_EXECUTABLE clang-tidy)
endif ()

if (NOT DEFINED COMPILE_COMMANDS_FILE)
    message(FATAL_ERROR "COMPILE_COMMANDS_FILE is required")
endif ()

if (NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif ()

if (NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif ()

if (NOT EXISTS "${COMPILE_COMMANDS_FILE}")
    message(FATAL_ERROR
        "Compilation database not found: ${COMPILE_COMMANDS_FILE}\n"
        "Configure with: cmake --preset ninja-lint")
endif ()

file(TO_CMAKE_PATH "${SOURCE_ROOT}" source_root)
if (NOT source_root MATCHES "/$")
    string(APPEND source_root "/")
endif ()

file(READ "${COMPILE_COMMANDS_FILE}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if (command_count EQUAL 0)
    message(FATAL_ERROR "No entries found in ${COMPILE_COMMANDS_FILE}")
endif ()

math(EXPR last_command_index "${command_count} - 1")
foreach (command_index RANGE 0 ${last_command_index})
    string(JSON source_file GET "${compile_commands_json}" ${command_index} file)
    file(TO_CMAKE_PATH "${source_file}" normalized_source_file)
    string(FIND "${normalized_source_file}" "${source_root}" source_root_position)

    if (source_root_position EQUAL 0 AND normalized_source_file MATCHES "\\.cpp$")
        list(APPEND source_files "${source_file}")
    endif ()
endforeach ()

list(LENGTH source_files source_file_count)
if (source_file_count EQUAL 0)
    message(FATAL_ERROR
        "No plugin source files found in ${COMPILE_COMMANDS_FILE}")
endif ()

message(STATUS "Running clang-tidy on ${source_file_count} plugin source files")

foreach (source_file IN LISTS source_files)
    file(RELATIVE_PATH relative_source_file "${SOURCE_ROOT}" "${source_file}")
    message(STATUS "clang-tidy ${relative_source_file}")

    execute_process(
        COMMAND "${CLANG_TIDY_EXECUTABLE}" --quiet -p "${BUILD_DIR}" "${source_file}"
        RESULT_VARIABLE clang_tidy_result
        OUTPUT_VARIABLE clang_tidy_output
        ERROR_VARIABLE clang_tidy_error)

    if (clang_tidy_output)
        message("${clang_tidy_output}")
    endif ()

    if (clang_tidy_error)
        message("${clang_tidy_error}")
    endif ()

    if (NOT clang_tidy_result EQUAL 0)
        list(APPEND failed_files "${relative_source_file}")
    endif ()
endforeach ()

if (failed_files)
    list(JOIN failed_files "\n  " failed_file_list)
    message(FATAL_ERROR
        "clang-tidy failed for:\n  ${failed_file_list}")
endif ()
