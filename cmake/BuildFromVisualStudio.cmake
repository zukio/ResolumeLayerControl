cmake_minimum_required(VERSION 3.15)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(binary "${root}/build-control-plugins")
if(NOT PLUGIN_TARGET MATCHES "^ffgl-plugin-(layer-(control|mask-control)|api-sender)$")
    message(FATAL_ERROR "Unknown plugin target: ${PLUGIN_TARGET}")
endif()
if(NOT CONFIG MATCHES "^(Debug|Release)$" OR NOT ACTION MATCHES "^(Build|Rebuild|Clean)$")
    message(FATAL_ERROR "Unsupported configuration/action")
endif()
# VS can start both wrapper projects concurrently. Serialize access to the
# shared CMake build tree, including configure, clean and generated headers.
file(MAKE_DIRECTORY "${binary}")
file(LOCK "${binary}/visual-studio.lock" GUARD PROCESS TIMEOUT 300)
function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Build command failed (${result})")
    endif()
endfunction()
set(python_option)
if(DEFINED PYTHON_EXE AND NOT PYTHON_EXE STREQUAL "")
    list(APPEND python_option "-DPython3_EXECUTABLE=${PYTHON_EXE}")
endif()
run("${CMAKE_COMMAND}" -S "${root}" -B "${binary}"
    -G "Visual Studio 17 2022" -A x64
    -DFFGL_BUILD_EXAMPLE_PLUGINS=OFF -DRLC_BUILD_PLUGINS=ON ${python_option})
if(ACTION STREQUAL "Clean" OR ACTION STREQUAL "Rebuild")
    # MSBuild target-scoped clean preserves the other plugin's final DLL.
    run("${CMAKE_COMMAND}" --build "${binary}" --config "${CONFIG}"
        --target "${PLUGIN_TARGET}" -- /t:Clean)
endif()
if(NOT ACTION STREQUAL "Clean")
    run("${CMAKE_COMMAND}" --build "${binary}" --config "${CONFIG}" --target "${PLUGIN_TARGET}")
endif()
