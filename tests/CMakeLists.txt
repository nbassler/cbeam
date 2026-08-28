find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE cbeam_core Qt::Test)
target_compile_options(test_model PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME model COMMAND test_model)
