include_guard(GLOBAL)
# The vendored GLEW archive uses the static release CRT. Keep every Windows
# object on that CRT, including Debug (symbols and /Od are still enabled).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
    add_compile_options(/utf-8)
endif()
