cmake_minimum_required(VERSION 3.16)

project(cbeam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# A bare `cmake -S . -B build` otherwise leaves the build type empty, which
# means neither optimisation nor debug symbols -- the worst of both. IDEs and
# multi-config generators choose for themselves and are left alone.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
endif()

# Lets clangd, and VS Code's IntelliSense, resolve includes without guessing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Which backend actually moves the rail.
#   sim   simulated motion, no hardware. Builds and runs anywhere.
#   gpio  drives the pins in src/config.h. Raspberry Pi only. Not written yet.
set(CBEAM_BACKEND "sim" CACHE STRING "Motion backend: sim or gpio")
set_property(CACHE CBEAM_BACKEND PROPERTY STRINGS sim gpio)

option(CBEAM_BUILD_TESTS "Build the test suite" ON)

if(CBEAM_BACKEND STREQUAL "gpio")
    message(FATAL_ERROR
        "CBEAM_BACKEND=gpio is not implemented. Motion is simulated for now; "
        "src/stepdriver.h is where a GPIO backend plugs in. "
        "Configure with -DCBEAM_BACKEND=sim.")
elseif(NOT CBEAM_BACKEND STREQUAL "sim")
    message(FATAL_ERROR
        "CBEAM_BACKEND must be 'sim' or 'gpio', not '${CBEAM_BACKEND}'.")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Git QUIET)

# The version shown in the window title comes from `git describe`, resolved on
# every build rather than at configure time so it cannot go stale after a
# commit. See cmake/GitVersion.cmake for the format.
set(CBEAM_VERSION_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(CBEAM_VERSION_HEADER ${CBEAM_VERSION_DIR}/version.h)

set(CBEAM_VERSION_COMMAND ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in
        -DOUT_FILE=${CBEAM_VERSION_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)

# Once now, so the header exists before anything tries to compile against it.
execute_process(COMMAND ${CBEAM_VERSION_COMMAND})

add_custom_target(cbeam_version
    COMMAND ${CBEAM_VERSION_COMMAND}
    BYPRODUCTS ${CBEAM_VERSION_HEADER}
    COMMENT "Resolving version from git"
    VERBATIM
)

# Position model and motion backend, split out of the executable so the tests
# link against exactly the code the app runs.
qt_add_library(cbeam_core STATIC
        src/config.h
        src/stepdriver.h
        src/model.h
        src/model.cpp
)
target_include_directories(cbeam_core PUBLIC src)
target_link_libraries(cbeam_core PUBLIC Qt::Core)

qt_add_executable(cbeam
        src/main.cpp
        src/slider.ui
        src/mainwindow.h
        src/mainwindow.cpp
)
target_link_libraries(cbeam PRIVATE
    cbeam_core
    Qt::Gui
    Qt::Widgets
)
add_dependencies(cbeam cbeam_version)
target_include_directories(cbeam PRIVATE ${CBEAM_VERSION_DIR})

foreach(target cbeam cbeam_core)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

message(STATUS "cbeam motion backend: ${CBEAM_BACKEND}")

if(CBEAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Convenience: `cmake --build build --target format` reformats everything in
# place. Style is stock LLVM, defined in .clang-format. CI checks it.
find_program(CBEAM_CLANG_FORMAT NAMES clang-format)

if(CBEAM_CLANG_FORMAT)
    file(GLOB_RECURSE CBEAM_FORMAT_SOURCES
         ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
         ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

    add_custom_target(format
        COMMAND ${CBEAM_CLANG_FORMAT} -i ${CBEAM_FORMAT_SOURCES}
        COMMENT "Reformatting sources with clang-format"
        VERBATIM
    )
endif()
