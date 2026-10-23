find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(RLC_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
function(rlc_generate_definition symbol)
  if(NOT TARGET generate-${symbol})
    add_custom_target(generate-${symbol}
      COMMAND "${Python3_EXECUTABLE}" "${RLC_ROOT}/tools/generate_plugin.py"
        --definition "${RLC_ROOT}/definitions/${symbol}.json"
        --output "${RLC_ROOT}/source/generated/${symbol}.h"
      VERBATIM)
  endif()
endfunction()
