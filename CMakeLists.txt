cmake_minimum_required(VERSION 3.5)

project(cbeam LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/slider.ui
        src/model.h
        src/model.cpp
        src/mainwindow.h
        src/mainwindow.cpp
)

qt_add_executable(cbeam
        ${PROJECT_SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wpedantic
)

target_link_libraries(cbeam  PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Widgets
)
