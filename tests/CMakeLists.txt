find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE cbeam_core Qt::Test)
target_compile_options(test_model PRIVATE -Wall -Wextra -Wpedantic)

# Register each QTest slot as its own ctest case rather than one opaque entry
# for the whole binary. Failures then name themselves, `ctest -R` can select a
# single case, and `ctest -j` runs them concurrently -- which matters here
# because the motion tests travel in real time.
#
# The list is read out of the source so it cannot fall behind: a test function
# that exists but was never registered would silently never run. Re-running
# cmake is wired to the file changing, so a new function registers itself.
set(CBEAM_TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/test_model.cpp)
set_property(DIRECTORY APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${CBEAM_TEST_SOURCE})

file(READ ${CBEAM_TEST_SOURCE} cbeam_test_text)
string(REGEX MATCHALL "void[ \t]+TestModel::([A-Za-z0-9_]+)[ \t]*\\("
       cbeam_test_definitions "${cbeam_test_text}")

foreach(definition ${cbeam_test_definitions})
    string(REGEX REPLACE "void[ \t]+TestModel::([A-Za-z0-9_]+)[ \t]*\\(" "\\1"
           case "${definition}")

    add_test(NAME model.${case} COMMAND test_model ${case})

    # Generous: the motion cases run travel at real speed, and CI machines are
    # slower than they look.
    set_tests_properties(model.${case} PROPERTIES TIMEOUT 120)
endforeach()

list(LENGTH cbeam_test_definitions cbeam_test_count)

if(cbeam_test_count EQUAL 0)
    message(FATAL_ERROR
        "No test cases found in ${CBEAM_TEST_SOURCE}. The regex that discovers "
        "them has probably gone stale -- it expects 'void TestModel::name()'.")
endif()

message(STATUS "cbeam test cases registered: ${cbeam_test_count}")
