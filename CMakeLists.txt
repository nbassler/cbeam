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
#   gpio  compiles in the pigpio driver as well, so the GUI's Simulation
#         tickbox can be turned off and the rail actually moves. Needs
#         libpigpiod-if-dev; Raspberry Pi 4 or earlier.
#
# This decides what is *available*, not what is *used*: a gpio build still
# starts in simulation, and hardware is opted into from the GUI.
set(CBEAM_BACKEND "sim" CACHE STRING "Motion backend: sim or gpio")
set_property(CACHE CBEAM_BACKEND PROPERTY STRINGS sim gpio)

option(CBEAM_BUILD_TESTS "Build the test suite" ON)

if(NOT CBEAM_BACKEND MATCHES "^(sim|gpio)$")
    message(FATAL_ERROR
        "CBEAM_BACKEND must be 'sim' or 'gpio', not '${CBEAM_BACKEND}'.")
endif()

# Qt 5 and Qt 6 both work, Qt 6 preferred when both are present.
#
# Qt 5 is supported because Pi OS Bullseye has no Qt 6 packages at all -- not
# an old version, none -- and reflashing a headless rig that is in use is not
# always an option. The cost is small: the only version-sensitive code is a
# handful of connects, and those use qOverload() so they resolve against
# either. Nothing here is Qt-6-only.
set(CBEAM_QT_VERSION "auto" CACHE STRING "Qt major version: auto, 5 or 6")
set_property(CACHE CBEAM_QT_VERSION PROPERTY STRINGS auto 5 6)

if(CBEAM_QT_VERSION STREQUAL "auto")
    # Probing for Qt6 explicitly rather than the usual
    # find_package(QT NAMES Qt6 Qt5 ...): in config mode CMake does not
    # reliably honour that name order, and on a machine with both installed it
    # picks Qt5, quietly building against the older one.
    find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)

    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
    else()
        set(QT_VERSION_MAJOR 5)
    endif()
elseif(CBEAM_QT_VERSION MATCHES "^[56]$")
    set(QT_VERSION_MAJOR ${CBEAM_QT_VERSION})
else()
    message(FATAL_ERROR
        "CBEAM_QT_VERSION must be auto, 5 or 6, not '${CBEAM_QT_VERSION}'.")
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
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
# Plain add_library/add_executable rather than qt_add_*: those exist only in
# Qt 6, and for a dynamically linked desktop build they add nothing that
# AUTOMOC and AUTOUIC are not already doing.
add_library(cbeam_core STATIC
        src/config.h
        src/stepdriver.h
        src/model.h
        src/model.cpp
)
target_include_directories(cbeam_core PUBLIC src)
target_link_libraries(cbeam_core PUBLIC Qt${QT_VERSION_MAJOR}::Core)

add_executable(cbeam
        src/main.cpp
        src/slider.ui
        src/mainwindow.h
        src/mainwindow.cpp
)
target_link_libraries(cbeam PRIVATE
    cbeam_core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)
add_dependencies(cbeam cbeam_version)
target_include_directories(cbeam PRIVATE ${CBEAM_VERSION_DIR})

if(CBEAM_BACKEND STREQUAL "gpio")
    find_path(PIGPIO_IF2_INCLUDE_DIR NAMES pigpiod_if2.h)
    find_library(PIGPIO_IF2_LIBRARY NAMES pigpiod_if2)

    if(NOT PIGPIO_IF2_INCLUDE_DIR OR NOT PIGPIO_IF2_LIBRARY)
        message(FATAL_ERROR
            "CBEAM_BACKEND=gpio needs the pigpio daemon client library.\n"
            "  sudo apt install libpigpiod-if-dev\n"
            "It exists only on Raspberry Pi (and pigpio itself does not "
            "support the Pi 5). Build with -DCBEAM_BACKEND=sim elsewhere.")
    endif()

    target_sources(cbeam PRIVATE src/pigpiodriver.h src/pigpiodriver.cpp)
    target_include_directories(cbeam PRIVATE ${PIGPIO_IF2_INCLUDE_DIR})
    target_link_libraries(cbeam PRIVATE ${PIGPIO_IF2_LIBRARY})
    target_compile_definitions(cbeam PRIVATE CBEAM_HAVE_GPIO)
endif()

foreach(target cbeam cbeam_core)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

message(STATUS "cbeam motion backend: ${CBEAM_BACKEND}")
message(STATUS "cbeam Qt version: ${Qt${QT_VERSION_MAJOR}_VERSION}")

if(CBEAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Packaging
#
# `cpack -G DEB` in the build tree produces an installable .deb.
#
# The Qt dependency is deliberately not written down here. CPACK_DEBIAN_
# PACKAGE_SHLIBDEPS runs dpkg-shlibdeps against the linked binary, so the same
# CMakeLists yields a package depending on libqt5widgets5 when built on
# Bullseye and libqt6widgets6 on Bookworm, without knowing which is which.
install(TARGETS cbeam RUNTIME DESTINATION bin)

# Package version is the nearest tag, resolved at configure time. This is not
# the same as CB_VERSION in the window title, which is resolved per build and
# carries the commit -- Debian versions have to be stable and start with a
# digit, so a bare tag is the right thing here.
set(CBEAM_PACKAGE_VERSION "0.0.0")

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE CBEAM_NEAREST_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE CBEAM_TAG_RESULT
    )

    if(CBEAM_TAG_RESULT EQUAL 0 AND CBEAM_NEAREST_TAG MATCHES "^v?([0-9].*)$")
        set(CBEAM_PACKAGE_VERSION "${CMAKE_MATCH_1}")
    endif()
endif()

set(CPACK_PACKAGE_NAME cbeam)
set(CPACK_PACKAGE_VERSION ${CBEAM_PACKAGE_VERSION})
set(CPACK_PACKAGE_VENDOR "Niels Bassler")
set(CPACK_PACKAGE_CONTACT "Niels Bassler <niels.bassler@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Controller for an OpenBuilds C-Beam linear actuator")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/nbassler/cbeam")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SECTION science)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Recommends rather than Depends: the daemon is what needs root and touches the
# hardware, but it can equally live on another Pi and be reached over the
# network via PIGPIO_ADDR. apt installs Recommends by default, so the ordinary
# case still just works.
if(CBEAM_BACKEND STREQUAL "gpio")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pigpiod")
endif()
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

message(STATUS "cbeam package version: ${CBEAM_PACKAGE_VERSION}")

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
