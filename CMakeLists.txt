cmake_minimum_required(VERSION 3.16)

project(cbeam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
