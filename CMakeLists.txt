cmake_minimum_required(VERSION 3.15)
project(FFGL-SDK
    VERSION 2.2
    LANGUAGES CXX
)
include(cmake/BuildOptions.cmake)

# resolve dependencies if using vcpkg
if (EXISTS CACHE{VCPKG_MANIFEST_FILE})
    vcpkg_acquire_dependencies()
endif()

add_library(ffgl-sdk STATIC
    source/lib/FFGLSDK.h
    source/lib/FFGLSDK.cpp
)

add_library(ffgl::sdk ALIAS ffgl-sdk)
set_target_properties(ffgl-sdk PROPERTIES EXPORT_NAME sdk)

target_include_directories(ffgl-sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source/lib/>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(ffgl-sdk PUBLIC cxx_std_11)

# we need glew (except on macOS)
if (NOT APPLE)
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_library(GLEW::GLEW STATIC IMPORTED)
        set_target_properties(GLEW::GLEW PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/deps/glew-2.1.0/lib/Release/x64/glew32s.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/deps/glew-2.1.0/include"
            INTERFACE_COMPILE_DEFINITIONS GLEW_STATIC)
    else()
        find_package(GLEW REQUIRED)
    endif()
    target_link_libraries(ffgl-sdk PUBLIC GLEW::GLEW)
    if(WIN32)
        target_link_libraries(ffgl-sdk PUBLIC opengl32)
    endif()
else()
    target_link_libraries(ffgl-sdk PUBLIC "-framework OpenGL" "-framework AppKit")
    set_target_properties(ffgl-sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

include(GNUInstallDirs)

install(
    DIRECTORY source/lib/ffgl
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    TARGETS     ffgl-sdk
    EXPORT      ffgl-sdk-targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    EXPORT      ffgl-sdk-targets
    NAMESPACE   ffgl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffgl-sdk
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ffgl-sdk/ffgl-sdk-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

export(
    EXPORT      ffgl-sdk-targets
    FILE        "${CMAKE_CURRENT_BINARY_DIR}/ffgl-sdk/ffgl-sdk-targets.cmake"
    NAMESPACE   ffgl::
)

configure_file(cmake/ffgl-sdk-config.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/ffgl-sdk/ffgl-sdk-config-version.cmake"
    COPYONLY
)

install(
    FILES
        cmake/ffgl-sdk-config.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/ffgl-sdk/ffgl-sdk-config-version.cmake"
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/ffgl-sdk
)

# are we building ffgl as the main repo, or is it included
# into another project?
set(FFGL_MASTER_PROJECT OFF)
if ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    set(FFGL_MASTER_PROJECT ON)
endif()

# should we build the example plugins? we do this by default
# if building the project directly, but not when adding ffgl
# to another project
option(FFGL_BUILD_EXAMPLE_PLUGINS "Build the example FFGL plugins" ${FFGL_MASTER_PROJECT})

if (${FFGL_BUILD_EXAMPLE_PLUGINS})
    add_subdirectory(source/plugins)
endif()

# Build the maintained plugins without pulling in the SDK examples.
option(RLC_BUILD_PLUGINS "Build Layer Control, Layer Mask Control and APISender" ON)
if(RLC_BUILD_PLUGINS AND NOT FFGL_BUILD_EXAMPLE_PLUGINS)
    add_subdirectory(source/plugins/LayerControl)
    add_subdirectory(source/plugins/LayerMaskControl)
    add_subdirectory(source/plugins/APISender)
endif()
